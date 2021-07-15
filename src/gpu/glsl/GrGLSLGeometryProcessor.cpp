/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/glsl/GrGLSLGeometryProcessor.h"

#include "src/core/SkMatrixPriv.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"
#include "src/gpu/glsl/GrGLSLVarying.h"
#include "src/gpu/glsl/GrGLSLVertexGeoBuilder.h"

GrGLSLGeometryProcessor::FPToVaryingCoordsMap GrGLSLGeometryProcessor::emitCode(
        EmitArgs& args,
        GrFragmentProcessor::CIter fpIter) {
    GrGPArgs gpArgs;
    this->onEmitCode(args, &gpArgs);

    FPToVaryingCoordsMap transformMap;
    if (gpArgs.fLocalCoordVar.getType() != kVoid_GrSLType) {
        transformMap = this->collectTransforms(args.fVertBuilder,
                                               args.fVaryingHandler,
                                               args.fUniformHandler,
                                               gpArgs.fLocalCoordVar,
                                               fpIter);
    }

    if (args.fGeomProc.willUseTessellationShaders()) {
        // Tessellation shaders are temporarily responsible for integrating their own code strings
        // while we work out full support.
        return transformMap;
    }

    GrGLSLVertexBuilder* vBuilder = args.fVertBuilder;
    if (!args.fGeomProc.willUseGeoShader()) {
        // Emit the vertex position to the hardware in the normalized window coordinates it expects.
        SkASSERT(kFloat2_GrSLType == gpArgs.fPositionVar.getType() ||
                 kFloat3_GrSLType == gpArgs.fPositionVar.getType());
        vBuilder->emitNormalizedSkPosition(gpArgs.fPositionVar.c_str(),
                                           gpArgs.fPositionVar.getType());
        if (kFloat2_GrSLType == gpArgs.fPositionVar.getType()) {
            args.fVaryingHandler->setNoPerspective();
        }
    } else {
        // Since we have a geometry shader, leave the vertex position in Skia device space for now.
        // The geometry Shader will operate in device space, and then convert the final positions to
        // normalized hardware window coordinates under the hood, once everything else has finished.
        // The subclass must call setNoPerspective on the varying handler, if applicable.
        vBuilder->codeAppendf("sk_Position = float4(%s", gpArgs.fPositionVar.c_str());
        switch (gpArgs.fPositionVar.getType()) {
            case kFloat_GrSLType:
                vBuilder->codeAppend(", 0");
                [[fallthrough]];
            case kFloat2_GrSLType:
                vBuilder->codeAppend(", 0");
                [[fallthrough]];
            case kFloat3_GrSLType:
                vBuilder->codeAppend(", 1");
                [[fallthrough]];
            case kFloat4_GrSLType:
                vBuilder->codeAppend(");");
                break;
            default:
                SK_ABORT("Invalid position var type");
                break;
        }
    }
    return transformMap;
}

GrGLSLGeometryProcessor::FPToVaryingCoordsMap GrGLSLGeometryProcessor::collectTransforms(
        GrGLSLVertexBuilder* vb,
        GrGLSLVaryingHandler* varyingHandler,
        GrGLSLUniformHandler* uniformHandler,
        const GrShaderVar& localCoordsVar,
        GrFragmentProcessor::CIter fpIter) {
    SkASSERT(localCoordsVar.getType() == kFloat2_GrSLType ||
             localCoordsVar.getType() == kFloat3_GrSLType);
    // Cached varyings produced by parent FPs. If parent FPs introduce transformations, but all
    // subsequent children are not transformed, they should share the same varying.
    std::unordered_map<const GrFragmentProcessor*, GrShaderVar> localCoordsMap;

    GrGLSLVarying baseLocalCoord;
    auto getBaseLocalCoord = [&baseLocalCoord, &localCoordsVar, vb, varyingHandler]() {
        SkASSERT(GrSLTypeIsFloatType(localCoordsVar.getType()));
        if (baseLocalCoord.type() == kVoid_GrSLType) {
            // Initialize to the GP provided coordinate
            SkString baseLocalCoordName = SkStringPrintf("LocalCoord");
            baseLocalCoord = GrGLSLVarying(localCoordsVar.getType());
            varyingHandler->addVarying(baseLocalCoordName.c_str(), &baseLocalCoord);
            vb->codeAppendf("%s = %s;\n", baseLocalCoord.vsOut(),
                            localCoordsVar.getName().c_str());
        }
        return baseLocalCoord.fsInVar();
    };

    FPToVaryingCoordsMap result;
    for (int i = 0; fpIter; ++fpIter, ++i) {
        const auto& fp = *fpIter;

        if (!fp.usesVaryingCoordsDirectly()) {
            continue;
        }

        // FPs that use local coordinates need a varying to convey the coordinate. This may be the
        // base GP's local coord if transforms have to be computed in the FS, or it may be a unique
        // varying that computes the equivalent transformation hierarchy in the VS.
        GrShaderVar varyingVar;

        // The FP's local coordinates are determined by the uniform transform hierarchy
        // from this FP to the root, and can be computed in the vertex shader.
        // If this hierarchy would be the identity transform, then we should use the
        // original local coordinate.
        // NOTE: The actual transform logic is handled in emitTransformCode(), this just
        // needs to determine if a unique varying should be added for the FP.
        GrShaderVar transformedLocalCoord;
        const GrFragmentProcessor* coordOwner = nullptr;

        const GrFragmentProcessor* node = &fp;
        while(node) {
            SkASSERT(!node->isSampledWithExplicitCoords());

            if (node->sampleUsage().isUniformMatrix()) {
                // We can stop once we hit an FP that adds transforms; this FP can reuse
                // that FPs varying (possibly vivifying it if this was the first use).
                transformedLocalCoord = localCoordsMap[node];
                coordOwner = node;
                break;
            } // else intervening FP is an identity transform so skip past it

            node = node->parent();
        }

        if (coordOwner) {
            // The FP will use coordOwner's varying; add varying if this was the first use
            if (transformedLocalCoord.getType() == kVoid_GrSLType) {
                GrGLSLVarying v(kFloat2_GrSLType);
                if (GrSLTypeVecLength(localCoordsVar.getType()) == 3 ||
                    coordOwner->hasPerspectiveTransform()) {
                    v = GrGLSLVarying(kFloat3_GrSLType);
                }
                SkString strVaryingName;
                strVaryingName.printf("TransformedCoords_%d", i);
                varyingHandler->addVarying(strVaryingName.c_str(), &v);

                fTransformInfos.push_back({v.vsOutVar(), localCoordsVar, coordOwner});
                transformedLocalCoord = v.fsInVar();
                localCoordsMap[coordOwner] = transformedLocalCoord;
            }

            varyingVar = transformedLocalCoord;
        } else {
            // The FP transform hierarchy is the identity, so use the original local coord
            varyingVar = getBaseLocalCoord();
        }

        SkASSERT(varyingVar.getType() != kVoid_GrSLType);
        result[&fp] = varyingVar;
    }
    return result;
}

void GrGLSLGeometryProcessor::emitTransformCode(GrGLSLVertexBuilder* vb,
                                                GrGLSLUniformHandler* uniformHandler) {
    std::unordered_map<const GrFragmentProcessor*, GrShaderVar> localCoordsMap;
    for (const auto& tr : fTransformInfos) {
        // If we recorded a transform info, its sample matrix must be uniform
        SkASSERT(tr.fFP->sampleUsage().isUniformMatrix());

        SkString localCoords;
        // Build a concatenated matrix expression that we apply to the root local coord.
        // If we have an expression cached from an early FP in the hierarchy chain, we can stop
        // there instead of going all the way to the GP.
        SkString transformExpression;

        const auto* base = tr.fFP;
        while(base) {
            GrShaderVar cachedBaseCoord = localCoordsMap[base];
            if (cachedBaseCoord.getType() != kVoid_GrSLType) {
                // Can stop here, as this varying already holds all transforms from higher FPs
                if (cachedBaseCoord.getType() == kFloat3_GrSLType) {
                    localCoords = cachedBaseCoord.getName();
                } else {
                    localCoords = SkStringPrintf("%s.xy1", cachedBaseCoord.getName().c_str());
                }
                break;
            } else if (base->sampleUsage().isUniformMatrix()) {
                // The matrix expression is always the same, but the parent defined the uniform
                GrShaderVar uniform = uniformHandler->liftUniformToVertexShader(
                        *base->parent(), SkString(SkSL::SampleUsage::MatrixUniformName()));
                SkASSERT(uniform.getType() == kFloat3x3_GrSLType);

                // Accumulate the base matrix expression as a preConcat
                if (!transformExpression.isEmpty()) {
                    transformExpression.append(" * ");
                }
                transformExpression.appendf("(%s)", uniform.getName().c_str());
            } else {
                // This intermediate FP is just a pass through and doesn't need to be built
                // in to the expression, but must visit its parents in case they add transforms
                SkASSERT(base->sampleUsage().isPassThrough() || !base->sampleUsage().isSampled());
            }

            base = base->parent();
        }

        if (localCoords.isEmpty()) {
            // Must use GP's local coords
            if (tr.fLocalCoords.getType() == kFloat3_GrSLType) {
                localCoords = tr.fLocalCoords.getName();
            } else {
                localCoords = SkStringPrintf("%s.xy1", tr.fLocalCoords.getName().c_str());
            }
        }

        vb->codeAppend("{\n");
        if (tr.fOutputCoords.getType() == kFloat2_GrSLType) {
            if (vb->getProgramBuilder()->shaderCaps()->nonsquareMatrixSupport()) {
                vb->codeAppendf("%s = float3x2(%s) * %s", tr.fOutputCoords.getName().c_str(),
                                                          transformExpression.c_str(),
                                                          localCoords.c_str());
            } else {
                vb->codeAppendf("%s = ((%s) * %s).xy", tr.fOutputCoords.getName().c_str(),
                                                       transformExpression.c_str(),
                                                       localCoords.c_str());
            }
        } else {
            SkASSERT(tr.fOutputCoords.getType() == kFloat3_GrSLType);
            vb->codeAppendf("%s = (%s) * %s", tr.fOutputCoords.getName().c_str(),
                                              transformExpression.c_str(),
                                              localCoords.c_str());
        }
        vb->codeAppend(";\n");
        vb->codeAppend("}\n");

        localCoordsMap.insert({ tr.fFP, tr.fOutputCoords });
    }
}

void GrGLSLGeometryProcessor::setupUniformColor(GrGLSLFPFragmentBuilder* fragBuilder,
                                                GrGLSLUniformHandler* uniformHandler,
                                                const char* outputName,
                                                UniformHandle* colorUniform) {
    SkASSERT(colorUniform);
    const char* stagedLocalVarName;
    *colorUniform = uniformHandler->addUniform(nullptr,
                                               kFragment_GrShaderFlag,
                                               kHalf4_GrSLType,
                                               "Color",
                                               &stagedLocalVarName);
    fragBuilder->codeAppendf("%s = %s;", outputName, stagedLocalVarName);
    if (fragBuilder->getProgramBuilder()->shaderCaps()->mustObfuscateUniformColor()) {
        fragBuilder->codeAppendf("%s = max(%s, half4(0));", outputName, outputName);
    }
}

void GrGLSLGeometryProcessor::SetTransform(const GrGLSLProgramDataManager& pdman,
                                           const GrShaderCaps& shaderCaps,
                                           const UniformHandle& uniform,
                                           const SkMatrix& matrix,
                                           SkMatrix* state) {
    if (!uniform.isValid() || (state && SkMatrixPriv::CheapEqual(*state, matrix))) {
        // No update needed
        return;
    }
    if (state) {
        *state = matrix;
    }
    if (matrix.isScaleTranslate() && !shaderCaps.reducedShaderMode()) {
        // ComputeMatrixKey and writeX() assume the uniform is a float4 (can't assert since nothing
        // is exposed on a handle, but should be caught lower down).
        float values[4] = {matrix.getScaleX(), matrix.getTranslateX(),
                           matrix.getScaleY(), matrix.getTranslateY()};
        pdman.set4fv(uniform, 1, values);
    } else {
        pdman.setSkMatrix(uniform, matrix);
    }
}

static void write_passthrough_vertex_position(GrGLSLVertexBuilder* vertBuilder,
                                              const GrShaderVar& inPos,
                                              GrShaderVar* outPos) {
    SkASSERT(inPos.getType() == kFloat3_GrSLType || inPos.getType() == kFloat2_GrSLType);
    SkString outName = vertBuilder->newTmpVarName(inPos.getName().c_str());
    outPos->set(inPos.getType(), outName.c_str());
    vertBuilder->codeAppendf("float%d %s = %s;",
                             GrSLTypeVecLength(inPos.getType()),
                             outName.c_str(),
                             inPos.getName().c_str());
}

static void write_vertex_position(GrGLSLVertexBuilder* vertBuilder,
                                  GrGLSLUniformHandler* uniformHandler,
                                  const GrShaderCaps& shaderCaps,
                                  const GrShaderVar& inPos,
                                  const SkMatrix& matrix,
                                  const char* matrixName,
                                  GrShaderVar* outPos,
                                  GrGLSLGeometryProcessor::UniformHandle* matrixUniform) {
    SkASSERT(inPos.getType() == kFloat3_GrSLType || inPos.getType() == kFloat2_GrSLType);
    SkString outName = vertBuilder->newTmpVarName(inPos.getName().c_str());

    if (matrix.isIdentity() && !shaderCaps.reducedShaderMode()) {
        write_passthrough_vertex_position(vertBuilder, inPos, outPos);
        return;
    }
    SkASSERT(matrixUniform);

    bool useCompactTransform = matrix.isScaleTranslate() && !shaderCaps.reducedShaderMode();
    const char* mangledMatrixName;
    *matrixUniform = uniformHandler->addUniform(nullptr,
                                                kVertex_GrShaderFlag,
                                                useCompactTransform ? kFloat4_GrSLType
                                                                    : kFloat3x3_GrSLType,
                                                matrixName,
                                                &mangledMatrixName);

    if (inPos.getType() == kFloat3_GrSLType) {
        // A float3 stays a float3 whether or not the matrix adds perspective
        if (useCompactTransform) {
            vertBuilder->codeAppendf("float3 %s = %s.xz1 * %s + %s.yw0;\n",
                                     outName.c_str(),
                                     mangledMatrixName,
                                     inPos.getName().c_str(),
                                     mangledMatrixName);
        } else {
            vertBuilder->codeAppendf("float3 %s = %s * %s;\n",
                                     outName.c_str(),
                                     mangledMatrixName,
                                     inPos.getName().c_str());
        }
        outPos->set(kFloat3_GrSLType, outName.c_str());
        return;
    }
    if (matrix.hasPerspective()) {
        // A float2 is promoted to a float3 if we add perspective via the matrix
        SkASSERT(!useCompactTransform);
        vertBuilder->codeAppendf("float3 %s = (%s * %s.xy1);",
                                 outName.c_str(),
                                 mangledMatrixName,
                                 inPos.getName().c_str());
        outPos->set(kFloat3_GrSLType, outName.c_str());
        return;
    }
    if (useCompactTransform) {
        vertBuilder->codeAppendf("float2 %s = %s.xz * %s + %s.yw;\n",
                                 outName.c_str(),
                                 mangledMatrixName,
                                 inPos.getName().c_str(),
                                 mangledMatrixName);
    } else if (shaderCaps.nonsquareMatrixSupport()) {
        vertBuilder->codeAppendf("float2 %s = float3x2(%s) * %s.xy1;\n",
                                 outName.c_str(),
                                 mangledMatrixName,
                                 inPos.getName().c_str());
    } else {
        vertBuilder->codeAppendf("float2 %s = (%s * %s.xy1).xy;\n",
                                 outName.c_str(),
                                 mangledMatrixName,
                                 inPos.getName().c_str());
    }
    outPos->set(kFloat2_GrSLType, outName.c_str());
}

void GrGLSLGeometryProcessor::WriteOutputPosition(GrGLSLVertexBuilder* vertBuilder,
                                                  GrGPArgs* gpArgs,
                                                  const char* posName) {
    // writeOutputPosition assumes the incoming pos name points to a float2 variable
    GrShaderVar inPos(posName, kFloat2_GrSLType);
    write_passthrough_vertex_position(vertBuilder, inPos, &gpArgs->fPositionVar);
}

void GrGLSLGeometryProcessor::WriteOutputPosition(GrGLSLVertexBuilder* vertBuilder,
                                                  GrGLSLUniformHandler* uniformHandler,
                                                  const GrShaderCaps& shaderCaps,
                                                  GrGPArgs* gpArgs,
                                                  const char* posName,
                                                  const SkMatrix& mat,
                                                  UniformHandle* viewMatrixUniform) {
    GrShaderVar inPos(posName, kFloat2_GrSLType);
    write_vertex_position(vertBuilder,
                          uniformHandler,
                          shaderCaps,
                          inPos,
                          mat,
                          "viewMatrix",
                          &gpArgs->fPositionVar,
                          viewMatrixUniform);
}

void GrGLSLGeometryProcessor::WriteLocalCoord(GrGLSLVertexBuilder* vertBuilder,
                                              GrGLSLUniformHandler* uniformHandler,
                                              const GrShaderCaps& shaderCaps,
                                              GrGPArgs* gpArgs,
                                              GrShaderVar localVar,
                                              const SkMatrix& localMatrix,
                                              UniformHandle* localMatrixUniform) {
    write_vertex_position(vertBuilder,
                          uniformHandler,
                          shaderCaps,
                          localVar,
                          localMatrix,
                          "localMatrix",
                          &gpArgs->fLocalCoordVar,
                          localMatrixUniform);
}
