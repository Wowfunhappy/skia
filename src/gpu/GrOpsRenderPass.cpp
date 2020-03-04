/*
* Copyright 2016 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#include "src/gpu/GrOpsRenderPass.h"

#include "include/core/SkRect.h"
#include "include/gpu/GrContext.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrFixedClip.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrMesh.h"
#include "src/gpu/GrPrimitiveProcessor.h"
#include "src/gpu/GrProgramInfo.h"
#include "src/gpu/GrRenderTarget.h"
#include "src/gpu/GrRenderTargetPriv.h"
#include "src/gpu/GrTexturePriv.h"

void GrOpsRenderPass::clear(const GrFixedClip& clip, const SkPMColor4f& color) {
    SkASSERT(fRenderTarget);
    // A clear at this level will always be a true clear, so make sure clears were not supposed to
    // be redirected to draws instead
    SkASSERT(!this->gpu()->caps()->performColorClearsAsDraws());
    SkASSERT(!clip.scissorEnabled() || !this->gpu()->caps()->performPartialClearsAsDraws());
    fDrawPipelineStatus = DrawPipelineStatus::kNotConfigured;
    this->onClear(clip, color);
}

void GrOpsRenderPass::clearStencilClip(const GrFixedClip& clip, bool insideStencilMask) {
    // As above, make sure the stencil clear wasn't supposed to be a draw rect with stencil settings
    SkASSERT(!this->gpu()->caps()->performStencilClearsAsDraws());
    fDrawPipelineStatus = DrawPipelineStatus::kNotConfigured;
    this->onClearStencilClip(clip, insideStencilMask);
}

void GrOpsRenderPass::executeDrawable(std::unique_ptr<SkDrawable::GpuDrawHandler> drawable) {
    fDrawPipelineStatus = DrawPipelineStatus::kNotConfigured;
    this->onExecuteDrawable(std::move(drawable));
}

void GrOpsRenderPass::bindPipeline(const GrProgramInfo& programInfo, const SkRect& drawBounds) {
#ifdef SK_DEBUG
    if (programInfo.primProc().hasInstanceAttributes()) {
         SkASSERT(this->gpu()->caps()->instanceAttribSupport());
    }
    if (programInfo.pipeline().usesConservativeRaster()) {
        SkASSERT(this->gpu()->caps()->conservativeRasterSupport());
        // Conservative raster, by default, only supports triangles. Implementations can
        // optionally indicate that they also support points and lines, but we don't currently
        // query or track that info.
        SkASSERT(GrIsPrimTypeTris(programInfo.primitiveType()));
    }
    if (programInfo.pipeline().isWireframe()) {
         SkASSERT(this->gpu()->caps()->wireframeSupport());
    }
    if (GrPrimitiveType::kPatches == programInfo.primitiveType()) {
        SkASSERT(this->gpu()->caps()->shaderCaps()->tessellationSupport());
    }
    programInfo.checkAllInstantiated();
    programInfo.checkMSAAAndMIPSAreResolved();
#endif

    if (programInfo.primProc().numVertexAttributes() > this->gpu()->caps()->maxVertexAttributes()) {
        fDrawPipelineStatus = DrawPipelineStatus::kFailedToBind;
        return;
    }

    if (!this->onBindPipeline(programInfo, drawBounds)) {
        fDrawPipelineStatus = DrawPipelineStatus::kFailedToBind;
        return;
    }

#ifdef SK_DEBUG
    GrProcessor::CustomFeatures processorFeatures = programInfo.requestedFeatures();
    if (GrProcessor::CustomFeatures::kSampleLocations & processorFeatures) {
        // Verify we always have the same sample pattern key, regardless of graphics state.
        SkASSERT(this->gpu()->findOrAssignSamplePatternKey(fRenderTarget)
                         == fRenderTarget->renderTargetPriv().getSamplePatternKey());
    }
    fScissorStatus = (programInfo.pipeline().isScissorTestEnabled()) ?
            DynamicStateStatus::kUninitialized : DynamicStateStatus::kDisabled;
    bool hasTextures = (programInfo.primProc().numTextureSamplers() > 0);
    if (!hasTextures) {
        programInfo.pipeline().visitProxies([&hasTextures](GrSurfaceProxy*, GrMipMapped) {
            hasTextures = true;
        });
    }
    fTextureBindingStatus = (hasTextures) ?
            DynamicStateStatus::kUninitialized : DynamicStateStatus::kDisabled;
    fHasIndexBuffer = false;
    fInstanceBufferStatus = (programInfo.primProc().hasInstanceAttributes()) ?
            DynamicStateStatus::kUninitialized : DynamicStateStatus::kDisabled;
    fVertexBufferStatus = (programInfo.primProc().hasVertexAttributes()) ?
            DynamicStateStatus::kUninitialized : DynamicStateStatus::kDisabled;
#endif

    fDrawPipelineStatus = DrawPipelineStatus::kOk;
    fXferBarrierType = programInfo.pipeline().xferBarrierType(fRenderTarget->asTexture(),
                                                              *this->gpu()->caps());
}

void GrOpsRenderPass::setScissorRect(const SkIRect& scissor) {
    if (DrawPipelineStatus::kOk != fDrawPipelineStatus) {
        SkASSERT(DrawPipelineStatus::kNotConfigured != fDrawPipelineStatus);
        return;
    }
    SkASSERT(DynamicStateStatus::kDisabled != fScissorStatus);
    this->onSetScissorRect(scissor);
    SkDEBUGCODE(fScissorStatus = DynamicStateStatus::kConfigured);
}

void GrOpsRenderPass::bindTextures(const GrPrimitiveProcessor& primProc, const GrPipeline& pipeline,
                                   const GrSurfaceProxy* const primProcTextures[]) {
    if (DrawPipelineStatus::kOk != fDrawPipelineStatus) {
        SkASSERT(DrawPipelineStatus::kNotConfigured != fDrawPipelineStatus);
        return;
    }
    SkASSERT((primProc.numTextureSamplers() > 0) == SkToBool(primProcTextures));
    // Don't assert on fTextureBindingStatus. onBindTextures() just turns into a no-op when there
    // aren't any textures, and it's hard to tell from the GrPipeline whether there are any. For
    // many clients it is easier to just always call this method.
    if (!this->onBindTextures(primProc, pipeline, primProcTextures)) {
        fDrawPipelineStatus = DrawPipelineStatus::kFailedToBind;
        return;
    }
    SkDEBUGCODE(fTextureBindingStatus = DynamicStateStatus::kConfigured);
}

void GrOpsRenderPass::drawMeshes(const GrProgramInfo& programInfo, const GrMesh meshes[],
                                 int meshCount) {
    if (programInfo.hasFixedScissor()) {
        this->setScissorRect(programInfo.fixedScissor());
    }
    if (!programInfo.hasDynamicPrimProcTextures()) {
        auto primProcTextures = (programInfo.hasFixedPrimProcTextures()) ?
                programInfo.fixedPrimProcTextures() : nullptr;
        this->bindTextures(programInfo.primProc(), programInfo.pipeline(), primProcTextures);
    }
    for (int i = 0; i < meshCount; ++i) {
        if (programInfo.hasDynamicScissors()) {
            this->setScissorRect(programInfo.dynamicScissor(i));
        }
        if (programInfo.hasDynamicPrimProcTextures()) {
            this->bindTextures(programInfo.primProc(), programInfo.pipeline(),
                               programInfo.dynamicPrimProcTextures(i));
        }
        meshes[i].draw(this);
    }
}

void GrOpsRenderPass::bindBuffers(const GrBuffer* indexBuffer, const GrBuffer* instanceBuffer,
                                  const GrBuffer* vertexBuffer, GrPrimitiveRestart primRestart) {
    if (DrawPipelineStatus::kOk != fDrawPipelineStatus) {
        SkASSERT(DrawPipelineStatus::kNotConfigured != fDrawPipelineStatus);
        return;
    }

#ifdef SK_DEBUG
    if (indexBuffer) {
        fHasIndexBuffer = true;
    }

    SkASSERT((DynamicStateStatus::kDisabled == fInstanceBufferStatus) != SkToBool(instanceBuffer));
    if (instanceBuffer) {
        fInstanceBufferStatus = DynamicStateStatus::kConfigured;
    }

    SkASSERT((DynamicStateStatus::kDisabled == fVertexBufferStatus) != SkToBool(vertexBuffer));
    if (vertexBuffer) {
        fVertexBufferStatus = DynamicStateStatus::kConfigured;
    }

    if (GrPrimitiveRestart::kYes == primRestart) {
        SkASSERT(this->gpu()->caps()->usePrimitiveRestart());
    }
#endif

    this->onBindBuffers(indexBuffer, instanceBuffer, vertexBuffer, primRestart);
}

bool GrOpsRenderPass::prepareToDraw() {
    if (DrawPipelineStatus::kOk != fDrawPipelineStatus) {
        SkASSERT(DrawPipelineStatus::kNotConfigured != fDrawPipelineStatus);
        this->gpu()->stats()->incNumFailedDraws();
        return false;
    }
    SkASSERT(DynamicStateStatus::kUninitialized != fScissorStatus);
    SkASSERT(DynamicStateStatus::kUninitialized != fTextureBindingStatus);

    if (kNone_GrXferBarrierType != fXferBarrierType) {
        this->gpu()->xferBarrier(fRenderTarget, fXferBarrierType);
    }
    return true;
}

void GrOpsRenderPass::draw(int vertexCount, int baseVertex) {
    if (!this->prepareToDraw()) {
        return;
    }
    SkASSERT(!fHasIndexBuffer);
    SkASSERT(DynamicStateStatus::kConfigured != fInstanceBufferStatus);
    SkASSERT(DynamicStateStatus::kUninitialized != fVertexBufferStatus);
    this->onDraw(vertexCount, baseVertex);
}

void GrOpsRenderPass::drawIndexed(int indexCount, int baseIndex, uint16_t minIndexValue,
                                  uint16_t maxIndexValue, int baseVertex) {
    if (!this->prepareToDraw()) {
        return;
    }
    SkASSERT(fHasIndexBuffer);
    SkASSERT(DynamicStateStatus::kConfigured != fInstanceBufferStatus);
    SkASSERT(DynamicStateStatus::kUninitialized != fVertexBufferStatus);
    this->onDrawIndexed(indexCount, baseIndex, minIndexValue, maxIndexValue, baseVertex);
}

void GrOpsRenderPass::drawInstanced(int instanceCount, int baseInstance, int vertexCount,
                                    int baseVertex) {
    if (!this->prepareToDraw()) {
        return;
    }
    SkASSERT(!fHasIndexBuffer);
    SkASSERT(DynamicStateStatus::kUninitialized != fInstanceBufferStatus);
    SkASSERT(DynamicStateStatus::kUninitialized != fVertexBufferStatus);
    this->onDrawInstanced(instanceCount, baseInstance, vertexCount, baseVertex);
}

void GrOpsRenderPass::drawIndexedInstanced(int indexCount, int baseIndex, int instanceCount,
                                           int baseInstance, int baseVertex) {
    if (!this->prepareToDraw()) {
        return;
    }
    SkASSERT(fHasIndexBuffer);
    SkASSERT(DynamicStateStatus::kUninitialized != fInstanceBufferStatus);
    SkASSERT(DynamicStateStatus::kUninitialized != fVertexBufferStatus);
    this->onDrawIndexedInstanced(indexCount, baseIndex, instanceCount, baseInstance, baseVertex);
}
