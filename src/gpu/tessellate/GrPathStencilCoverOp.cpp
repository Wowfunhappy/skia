/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/tessellate/GrPathStencilCoverOp.h"

#include "src/gpu/GrEagerVertexAllocator.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrResourceProvider.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLVarying.h"
#include "src/gpu/glsl/GrGLSLVertexGeoBuilder.h"
#include "src/gpu/tessellate/GrMiddleOutPolygonTriangulator.h"
#include "src/gpu/tessellate/GrPathCurveTessellator.h"
#include "src/gpu/tessellate/GrPathWedgeTessellator.h"
#include "src/gpu/tessellate/GrTessellationPathRenderer.h"
#include "src/gpu/tessellate/shaders/GrPathTessellationShader.h"

using PathFlags = GrTessellationPathRenderer::PathFlags;

namespace {

// Fills a path's bounding box, with subpixel outset to avoid possible T-junctions with extreme
// edges of the path.
// NOTE: The emitted geometry may not be axis-aligned, depending on the view matrix.
class BoundingBoxShader : public GrGeometryProcessor {
public:
    BoundingBoxShader(SkPMColor4f color, const GrShaderCaps& shaderCaps)
            : GrGeometryProcessor(kTessellate_BoundingBoxShader_ClassID)
            , fColor(color) {
        if (!shaderCaps.vertexIDSupport()) {
            constexpr static Attribute kUnitCoordAttrib("unitCoord", kFloat2_GrVertexAttribType,
                                                        kFloat2_GrSLType);
            this->setVertexAttributes(&kUnitCoordAttrib, 1);
        }
        constexpr static Attribute kInstanceAttribs[] = {
            {"matrix2d", kFloat4_GrVertexAttribType, kFloat4_GrSLType},
            {"translate", kFloat2_GrVertexAttribType, kFloat2_GrSLType},
            {"pathBounds", kFloat4_GrVertexAttribType, kFloat4_GrSLType}
        };
        this->setInstanceAttributes(kInstanceAttribs, SK_ARRAY_COUNT(kInstanceAttribs));
    }

private:
    const char* name() const final { return "tessellate_BoundingBoxShader"; }
    void getGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const final {}
    GrGLSLGeometryProcessor* createGLSLInstance(const GrShaderCaps&) const final;

    const SkPMColor4f fColor;
};

GrGLSLGeometryProcessor* BoundingBoxShader::createGLSLInstance(const GrShaderCaps&) const {
    class Impl : public GrGLSLGeometryProcessor {
        void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) final {
            args.fVaryingHandler->emitAttributes(args.fGeomProc);

            // Vertex shader.
            if (args.fShaderCaps->vertexIDSupport()) {
                // If we don't have sk_VertexID support then "unitCoord" already came in as a vertex
                // attrib.
                args.fVertBuilder->codeAppend(R"(
                float2 unitCoord = float2(sk_VertexID & 1, sk_VertexID >> 1);)");
            }
            args.fVertBuilder->codeAppend(R"(
            // Bloat the bounding box by 1/4px to be certain we will reset every stencil value.
            float2x2 M_ = inverse(float2x2(matrix2d));
            float2 bloat = float2(abs(M_[0]) + abs(M_[1])) * .25;

            // Find the vertex position.
            float2 localcoord = mix(pathBounds.xy - bloat, pathBounds.zw + bloat, unitCoord);
            float2 vertexpos = float2x2(matrix2d) * localcoord + translate;)");
            gpArgs->fLocalCoordVar.set(kFloat2_GrSLType, "localcoord");
            gpArgs->fPositionVar.set(kFloat2_GrSLType, "vertexpos");

            // Fragment shader.
            const char* color;
            fColorUniform = args.fUniformHandler->addUniform(nullptr, kFragment_GrShaderFlag,
                                                             kHalf4_GrSLType, "color", &color);
            args.fFragBuilder->codeAppendf("half4 %s = %s;", args.fOutputColor, color);
            args.fFragBuilder->codeAppendf("const half4 %s = half4(1);", args.fOutputCoverage);
        }

        void setData(const GrGLSLProgramDataManager& pdman, const GrShaderCaps&,
                     const GrGeometryProcessor& gp) override {
            const SkPMColor4f& color = gp.cast<BoundingBoxShader>().fColor;
            pdman.set4f(fColorUniform, color.fR, color.fG, color.fB, color.fA);
        }

        GrGLSLUniformHandler::UniformHandle fColorUniform;
    };

    return new Impl;
}

}  // namespace

void GrPathStencilCoverOp::visitProxies(const GrVisitProxyFunc& func) const {
    if (fCoverBBoxProgram) {
        fCoverBBoxProgram->pipeline().visitProxies(func);
    } else {
        fProcessors.visitProxies(func);
    }
}

GrDrawOp::FixedFunctionFlags GrPathStencilCoverOp::fixedFunctionFlags() const {
    auto flags = FixedFunctionFlags::kUsesStencil;
    if (fAAType != GrAAType::kNone) {
        flags |= FixedFunctionFlags::kUsesHWAA;
    }
    return flags;
}

GrProcessorSet::Analysis GrPathStencilCoverOp::finalize(const GrCaps& caps,
                                                        const GrAppliedClip* clip,
                                                        GrClampType clampType) {
    return fProcessors.finalize(fColor, GrProcessorAnalysisCoverage::kNone, clip, nullptr, caps,
                                clampType, &fColor);
}

void GrPathStencilCoverOp::prePreparePrograms(const GrTessellationShader::ProgramArgs& args,
                                              GrAppliedClip&& appliedClip) {
    SkASSERT(!fTessellator);
    SkASSERT(!fStencilFanProgram);
    SkASSERT(!fStencilPathProgram);
    SkASSERT(!fCoverBBoxProgram);

    // We transform paths on the CPU. This allows for better batching.
    const SkMatrix& shaderMatrix = SkMatrix::I();
    const GrPipeline* stencilPipeline = GrPathTessellationShader::MakeStencilOnlyPipeline(
            args, fAAType, fPathFlags, appliedClip.hardClip());
    const GrUserStencilSettings* stencilPathSettings =
            GrPathTessellationShader::StencilPathSettings(GrFillRuleForSkPath(fPath));

    if (fPath.countVerbs() > 50 && this->bounds().height() * this->bounds().width() > 256 * 256) {
        // Large complex paths do better with a dedicated triangle shader for the inner fan.
        // This takes less PCI bus bandwidth (6 floats per triangle instead of 8) and allows us
        // to make sure it has an efficient middle-out topology.
        auto shader = GrPathTessellationShader::MakeSimpleTriangleShader(args.fArena,
                                                                         shaderMatrix,
                                                                         SK_PMColor4fTRANSPARENT);
        fStencilFanProgram = GrTessellationShader::MakeProgram(args,
                                                               shader,
                                                               stencilPipeline,
                                                               stencilPathSettings);
        fTessellator = GrPathCurveTessellator::Make(args.fArena,
                                                    shaderMatrix,
                                                    SK_PMColor4fTRANSPARENT,
                                                    GrPathCurveTessellator::DrawInnerFan::kNo,
                                                    fPath.countVerbs(),
                                                    *stencilPipeline,
                                                    *args.fCaps);
    } else {
        fTessellator = GrPathWedgeTessellator::Make(args.fArena,
                                                    shaderMatrix,
                                                    SK_PMColor4fTRANSPARENT,
                                                    fPath.countVerbs(),
                                                    *stencilPipeline,
                                                    *args.fCaps);
    }
    fStencilPathProgram = GrTessellationShader::MakeProgram(args, fTessellator->shader(),
                                                            stencilPipeline, stencilPathSettings);

    if (!(fPathFlags & PathFlags::kStencilOnly)) {
        // Create a program that draws a bounding box over the path and fills its stencil coverage
        // into the color buffer.
        auto* bboxShader = args.fArena->make<BoundingBoxShader>(fColor, *args.fCaps->shaderCaps());
        auto* bboxPipeline = GrTessellationShader::MakePipeline(args, fAAType,
                                                                std::move(appliedClip),
                                                                std::move(fProcessors));
        auto* bboxStencil =
                GrPathTessellationShader::TestAndResetStencilSettings(fPath.isInverseFillType());
        fCoverBBoxProgram = GrSimpleMeshDrawOpHelper::CreateProgramInfo(
                args.fArena,
                bboxPipeline,
                args.fWriteView,
                bboxShader,
                GrPrimitiveType::kTriangleStrip,
                args.fXferBarrierFlags,
                args.fColorLoadOp,
                bboxStencil);
    }
}

void GrPathStencilCoverOp::onPrePrepare(GrRecordingContext* context,
                                        const GrSurfaceProxyView& writeView, GrAppliedClip* clip,
                                        const GrDstProxyView& dstProxyView,
                                        GrXferBarrierFlags renderPassXferBarriers,
                                        GrLoadOp colorLoadOp) {
    this->prePreparePrograms({context->priv().recordTimeAllocator(), writeView, &dstProxyView,
                             renderPassXferBarriers, colorLoadOp, context->priv().caps()},
                             (clip) ? std::move(*clip) : GrAppliedClip::Disabled());
    if (fStencilFanProgram) {
        context->priv().recordProgramInfo(fStencilFanProgram);
    }
    if (fStencilPathProgram) {
        context->priv().recordProgramInfo(fStencilPathProgram);
    }
    if (fCoverBBoxProgram) {
        context->priv().recordProgramInfo(fCoverBBoxProgram);
    }
}

GR_DECLARE_STATIC_UNIQUE_KEY(gUnitQuadBufferKey);

void GrPathStencilCoverOp::onPrepare(GrOpFlushState* flushState) {
    if (!fTessellator) {
        this->prePreparePrograms({flushState->allocator(), flushState->writeView(),
                                  &flushState->dstProxyView(), flushState->renderPassBarriers(),
                                  flushState->colorLoadOp(), &flushState->caps()},
                                  flushState->detachAppliedClip());
        if (!fTessellator) {
            return;
        }
    }

    // We transform paths on the CPU. This allows for better batching.
    const SkMatrix& pathMatrix = fViewMatrix;

    if (fStencilFanProgram) {
        // The inner fan isn't built into the tessellator. Generate a standard Redbook fan with a
        // middle-out topology.
        GrEagerDynamicVertexAllocator vertexAlloc(flushState, &fFanBuffer, &fFanBaseVertex);
        int maxFanTriangles = fPath.countVerbs() - 2;  // n - 2 triangles make an n-gon.
        GrVertexWriter triangleVertexWriter = vertexAlloc.lock<SkPoint>(maxFanTriangles * 3);
        int numTrianglesWritten;
        GrMiddleOutPolygonTriangulator::WritePathInnerFan(std::move(triangleVertexWriter),
                                                          0,
                                                          0,
                                                          pathMatrix,
                                                          fPath,
                                                          &numTrianglesWritten);
        fFanVertexCount = 3 * numTrianglesWritten;
        SkASSERT(fFanVertexCount <= maxFanTriangles * 3);
        vertexAlloc.unlock(fFanVertexCount);
    }

    fTessellator->prepare(flushState, this->bounds(), pathMatrix, fPath);

    if (fCoverBBoxProgram) {
        size_t instanceStride = fCoverBBoxProgram->geomProc().instanceStride();
        GrVertexWriter vertexWriter = flushState->makeVertexSpace(
                instanceStride,
                1,
                &fBBoxBuffer,
                &fBBoxBaseInstance);
        SkDEBUGCODE(auto end = vertexWriter.makeOffset(instanceStride));
        vertexWriter.write(fViewMatrix.getScaleX(),
                           fViewMatrix.getSkewY(),
                           fViewMatrix.getSkewX(),
                           fViewMatrix.getScaleY(),
                           fViewMatrix.getTranslateX(),
                           fViewMatrix.getTranslateY());
        if (fPath.isInverseFillType()) {
            // Fill the entire backing store to make sure we clear every stencil value back to 0. If
            // there is a scissor it will have already clipped the stencil draw.
            auto rtBounds = flushState->writeView().asRenderTargetProxy()->backingStoreBoundsRect();
            SkASSERT(rtBounds == fOriginalDrawBounds);
            SkRect pathSpaceRTBounds;
            if (SkMatrixPriv::InverseMapRect(fViewMatrix, &pathSpaceRTBounds, rtBounds)) {
                vertexWriter.write(pathSpaceRTBounds);
            } else {
                vertexWriter.write(fPath.getBounds());
            }
        } else {
            vertexWriter.write(fPath.getBounds());
        }
        SkASSERT(vertexWriter == end);
    }

    if (!flushState->caps().shaderCaps()->vertexIDSupport()) {
        constexpr static SkPoint kUnitQuad[4] = {{0,0}, {0,1}, {1,0}, {1,1}};

        GR_DEFINE_STATIC_UNIQUE_KEY(gUnitQuadBufferKey);

        fBBoxVertexBufferIfNoIDSupport = flushState->resourceProvider()->findOrMakeStaticBuffer(
                GrGpuBufferType::kVertex, sizeof(kUnitQuad), kUnitQuad, gUnitQuadBufferKey);
    }
}

void GrPathStencilCoverOp::onExecute(GrOpFlushState* flushState, const SkRect& chainBounds) {
    if (!fTessellator) {
        return;
    }

    // Stencil the inner fan, if any.
    if (fFanVertexCount > 0) {
        SkASSERT(fStencilFanProgram);
        SkASSERT(fFanBuffer);
        flushState->bindPipelineAndScissorClip(*fStencilFanProgram, this->bounds());
        flushState->bindBuffers(nullptr, nullptr, fFanBuffer);
        flushState->draw(fFanVertexCount, fFanBaseVertex);
    }

    // Stencil the rest of the path.
    SkASSERT(fStencilPathProgram);
    flushState->bindPipelineAndScissorClip(*fStencilPathProgram, this->bounds());
    fTessellator->draw(flushState);
    if (flushState->caps().requiresManualFBBarrierAfterTessellatedStencilDraw()) {
        flushState->gpu()->insertManualFramebufferBarrier();  // http://skbug.com/9739
    }

    // Fill in the bounding box (if not in stencil-only mode).
    if (fCoverBBoxProgram) {
        flushState->bindPipelineAndScissorClip(*fCoverBBoxProgram, this->bounds());
        flushState->bindTextures(fCoverBBoxProgram->geomProc(), nullptr,
                                 fCoverBBoxProgram->pipeline());
        flushState->bindBuffers(nullptr, fBBoxBuffer, fBBoxVertexBufferIfNoIDSupport);
        flushState->drawInstanced(1, fBBoxBaseInstance, 4, 0);
    }
}
