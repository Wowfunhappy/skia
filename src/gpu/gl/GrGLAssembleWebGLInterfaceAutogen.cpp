/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * THIS FILE IS AUTOGENERATED
 * Make edits to tools/gpu/gl/interface/templates.go or they will
 * be overwritten.
 */

#include "include/gpu/gl/GrGLAssembleHelpers.h"
#include "include/gpu/gl/GrGLAssembleInterface.h"
#include "src/gpu/gl/GrGLUtil.h"

#if SK_DISABLE_WEBGL_INTERFACE || !defined(SK_USE_WEBGL)
sk_sp<const GrGLInterface> GrGLMakeAssembledWebGLInterface(void *ctx, GrGLGetProc get) {
    return nullptr;
}
#else

// Located https://github.com/emscripten-core/emscripten/tree/7ba7700902c46734987585409502f3c63beb650f/system/include/webgl
#include <webgl/webgl1.h>
#include <webgl/webgl1_ext.h>
#include <webgl/webgl2.h>
#include <webgl/webgl2_ext.h>

#define GET_PROC(F) functions->f##F = emscripten_gl##F
#define GET_PROC_SUFFIX(F, S) functions->f##F = emscripten_gl##F##S

// Adapter from standard GL signature to emscripten.
void emscripten_glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    uint32_t timeoutLo = timeout;
    uint32_t timeoutHi = timeout >> 32;
    emscripten_glWaitSync(sync, flags, timeoutLo, timeoutHi);
}

// Adapter from standard GL signature to emscripten.
GLenum emscripten_glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    uint32_t timeoutLo = timeout;
    uint32_t timeoutHi = timeout >> 32;
    return emscripten_glClientWaitSync(sync, flags, timeoutLo, timeoutHi);
}

sk_sp<const GrGLInterface> GrGLMakeAssembledWebGLInterface(void *ctx, GrGLGetProc get) {
    const char* verStr = reinterpret_cast<const char*>(emscripten_glGetString(GR_GL_VERSION));
    GrGLVersion glVer = GrGLGetVersionFromString(verStr);
    if (glVer < GR_GL_VER(1,0)) {
        return nullptr;
    }

    GrGLExtensions extensions;
    if (!extensions.init(kWebGL_GrGLStandard, emscripten_glGetString, emscripten_glGetStringi,
                         emscripten_glGetIntegerv)) {
        return nullptr;
    }

    sk_sp<GrGLInterface> interface(new GrGLInterface);
    GrGLInterface::Functions* functions = &interface->fFunctions;

    // Autogenerated content follows
    GET_PROC(ActiveTexture);
    GET_PROC(AttachShader);
    GET_PROC(BindAttribLocation);
    GET_PROC(BindBuffer);
    GET_PROC(BindTexture);
    GET_PROC(BlendColor);
    GET_PROC(BlendEquation);
    GET_PROC(BlendFunc);
    GET_PROC(BufferData);
    GET_PROC(BufferSubData);
    GET_PROC(Clear);
    GET_PROC(ClearColor);
    GET_PROC(ClearStencil);
    GET_PROC(ColorMask);
    GET_PROC(CompileShader);
    GET_PROC(CompressedTexImage2D);
    GET_PROC(CompressedTexSubImage2D);
    GET_PROC(CopyTexSubImage2D);
    GET_PROC(CreateProgram);
    GET_PROC(CreateShader);
    GET_PROC(CullFace);
    GET_PROC(DeleteBuffers);
    GET_PROC(DeleteProgram);
    GET_PROC(DeleteShader);
    GET_PROC(DeleteTextures);
    GET_PROC(DepthMask);
    GET_PROC(Disable);
    GET_PROC(DisableVertexAttribArray);
    GET_PROC(DrawArrays);
    GET_PROC(DrawElements);
    GET_PROC(Enable);
    GET_PROC(EnableVertexAttribArray);
    GET_PROC(Finish);
    GET_PROC(Flush);
    GET_PROC(FrontFace);
    GET_PROC(GenBuffers);
    GET_PROC(GenTextures);
    GET_PROC(GetBufferParameteriv);
    GET_PROC(GetError);
    GET_PROC(GetFloatv);
    GET_PROC(GetIntegerv);
    GET_PROC(GetProgramInfoLog);
    GET_PROC(GetProgramiv);
    GET_PROC(GetShaderInfoLog);
    GET_PROC(GetShaderiv);
    GET_PROC(GetString);
    GET_PROC(GetUniformLocation);
    GET_PROC(IsTexture);
    GET_PROC(LineWidth);
    GET_PROC(LinkProgram);
    GET_PROC(PixelStorei);
    GET_PROC(ReadPixels);
    GET_PROC(Scissor);
    GET_PROC(ShaderSource);
    GET_PROC(StencilFunc);
    GET_PROC(StencilFuncSeparate);
    GET_PROC(StencilMask);
    GET_PROC(StencilMaskSeparate);
    GET_PROC(StencilOp);
    GET_PROC(StencilOpSeparate);
    GET_PROC(TexImage2D);
    GET_PROC(TexParameterf);
    GET_PROC(TexParameterfv);
    GET_PROC(TexParameteri);
    GET_PROC(TexParameteriv);
    GET_PROC(TexSubImage2D);
    GET_PROC(Uniform1f);
    GET_PROC(Uniform1fv);
    GET_PROC(Uniform1i);
    GET_PROC(Uniform1iv);
    GET_PROC(Uniform2f);
    GET_PROC(Uniform2fv);
    GET_PROC(Uniform2i);
    GET_PROC(Uniform2iv);
    GET_PROC(Uniform3f);
    GET_PROC(Uniform3fv);
    GET_PROC(Uniform3i);
    GET_PROC(Uniform3iv);
    GET_PROC(Uniform4f);
    GET_PROC(Uniform4fv);
    GET_PROC(Uniform4i);
    GET_PROC(Uniform4iv);
    GET_PROC(UniformMatrix2fv);
    GET_PROC(UniformMatrix3fv);
    GET_PROC(UniformMatrix4fv);
    GET_PROC(UseProgram);
    GET_PROC(VertexAttrib1f);
    GET_PROC(VertexAttrib2fv);
    GET_PROC(VertexAttrib3fv);
    GET_PROC(VertexAttrib4fv);
    GET_PROC(VertexAttribPointer);
    GET_PROC(Viewport);

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(GetStringi);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(BindVertexArray);
        GET_PROC(DeleteVertexArrays);
        GET_PROC(GenVertexArrays);
    } else if (extensions.has("GL_OES_vertex_array_object")) {
        GET_PROC_SUFFIX(BindVertexArray, OES);
        GET_PROC_SUFFIX(DeleteVertexArrays, OES);
        GET_PROC_SUFFIX(GenVertexArrays, OES);
    } else if (extensions.has("OES_vertex_array_object")) {
        GET_PROC_SUFFIX(BindVertexArray, OES);
        GET_PROC_SUFFIX(DeleteVertexArrays, OES);
        GET_PROC_SUFFIX(GenVertexArrays, OES);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(DrawArraysInstanced);
        GET_PROC(DrawElementsInstanced);
    }

    if (extensions.has("GL_WEBGL_draw_instanced_base_vertex_base_instance")) {
        GET_PROC_SUFFIX(DrawArraysInstancedBaseInstance, WEBGL);
        GET_PROC_SUFFIX(DrawElementsInstancedBaseVertexBaseInstance, WEBGL);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(DrawBuffers);
        GET_PROC(ReadBuffer);
    }

    if (extensions.has("GL_WEBGL_multi_draw_instanced_base_vertex_base_instance")) {
        GET_PROC_SUFFIX(MultiDrawArraysInstancedBaseInstance, WEBGL);
        GET_PROC_SUFFIX(MultiDrawElementsInstancedBaseVertexBaseInstance, WEBGL);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(DrawRangeElements);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(TexStorage2D);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(VertexAttribDivisor);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(VertexAttribIPointer);
    }

    GET_PROC(BindFramebuffer);
    GET_PROC(BindRenderbuffer);
    GET_PROC(CheckFramebufferStatus);
    GET_PROC(DeleteFramebuffers);
    GET_PROC(DeleteRenderbuffers);
    GET_PROC(FramebufferRenderbuffer);
    GET_PROC(FramebufferTexture2D);
    GET_PROC(GenFramebuffers);
    GET_PROC(GenRenderbuffers);
    GET_PROC(GenerateMipmap);
    GET_PROC(GetFramebufferAttachmentParameteriv);
    GET_PROC(GetRenderbufferParameteriv);
    GET_PROC(RenderbufferStorage);

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(BlitFramebuffer);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(RenderbufferStorageMultisample);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(ClientWaitSync);
        GET_PROC(DeleteSync);
        GET_PROC(FenceSync);
        GET_PROC(IsSync);
        GET_PROC(WaitSync);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(BindSampler);
        GET_PROC(DeleteSamplers);
        GET_PROC(GenSamplers);
        GET_PROC(SamplerParameterf);
        GET_PROC(SamplerParameteri);
        GET_PROC(SamplerParameteriv);
    }

    if (glVer >= GR_GL_VER(2,0)) {
        GET_PROC(InvalidateFramebuffer);
        GET_PROC(InvalidateSubFramebuffer);
    }

    GET_PROC(GetShaderPrecisionFormat);


    // End autogenerated content

    interface->fStandard = kWebGL_GrGLStandard;
    interface->fExtensions.swap(&extensions);

    return std::move(interface);
}
#endif
