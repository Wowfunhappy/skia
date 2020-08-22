/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrHighContrastFilterEffect.fp; do not modify.
 **************************************************************************************************/
#include "GrHighContrastFilterEffect.h"

#include "src/core/SkUtils.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"
class GrGLSLHighContrastFilterEffect : public GrGLSLFragmentProcessor {
public:
    GrGLSLHighContrastFilterEffect() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrHighContrastFilterEffect& _outer = args.fFp.cast<GrHighContrastFilterEffect>();
        (void)_outer;
        auto contrastMod = _outer.contrastMod;
        (void)contrastMod;
        auto hasContrast = _outer.hasContrast;
        (void)hasContrast;
        auto grayscale = _outer.grayscale;
        (void)grayscale;
        auto invertBrightness = _outer.invertBrightness;
        (void)invertBrightness;
        auto invertLightness = _outer.invertLightness;
        (void)invertLightness;
        auto linearize = _outer.linearize;
        (void)linearize;
        contrastModVar = args.fUniformHandler->addUniform(&_outer, kFragment_GrShaderFlag,
                                                          kHalf_GrSLType, "contrastMod");
        SkString HSLToRGB_name;
        const GrShaderVar HSLToRGB_args[] = {GrShaderVar("p", kHalf_GrSLType),
                                             GrShaderVar("q", kHalf_GrSLType),
                                             GrShaderVar("t", kHalf_GrSLType)};
        fragBuilder->emitFunction(kHalf_GrSLType, "HSLToRGB", 3, HSLToRGB_args,
                                  R"SkSL(if (t < 0.0) t += 1.0;
if (t > 1.0) t -= 1.0;
return t < 0.16666666666666666 ? p + ((q - p) * 6.0) * t : (t < 0.5 ? q : (t < 0.66666666666666663 ? p + ((q - p) * (0.66666666666666663 - t)) * 6.0 : p));
)SkSL",
                                  &HSLToRGB_name);
        SkString _sample896 = this->invokeChild(0, args);
        fragBuilder->codeAppendf(
                R"SkSL(
half4 inColor = %s;
half4 _inlineResulthalf4unpremulhalf40;
half4 _inlineArghalf4unpremulhalf41_0 = inColor;
{
    _inlineResulthalf4unpremulhalf40 = half4(_inlineArghalf4unpremulhalf41_0.xyz / max(_inlineArghalf4unpremulhalf41_0.w, 9.9999997473787516e-05), _inlineArghalf4unpremulhalf41_0.w);
}
half4 color = _inlineResulthalf4unpremulhalf40;

@if (%s) {
    color.xyz = color.xyz * color.xyz;
}
@if (%s) {
    color = half4(half3(dot(color.xyz, half3(0.2125999927520752, 0.71520000696182251, 0.072200000286102295))), 0.0);
}
@if (%s) {
    color = half4(1.0) - color;
}
@if (%s) {
    half fmax = max(color.x, max(color.y, color.z));
    half fmin = min(color.x, min(color.y, color.z));
    half l = fmax + fmin;
    half h;
    half s;
    if (fmax == fmin) {
        h = 0.0;
        s = 0.0;
    } else {
        half d = fmax - fmin;
        s = l > 1.0 ? d / (2.0 - l) : d / l;
        if (color.x >= color.y && color.x >= color.z) {
            h = (color.y - color.z) / d + half(color.y < color.z ? 6 : 0);
        } else if (color.y >= color.z) {
            h = (color.z - color.x) / d + 2.0;
        } else {
            h = (color.x - color.y) / d + 4.0;
        }
        h *= 0.16666666666666666;
    }
    l = 1.0 + l * -0.5;
    if (s == 0.0) {
        color = half4(l, l, l, 0.0);
    } else {
        half q = l < 0.5 ? l * (1.0 + s) : (l + s) - l * s;
        half p = 2.0 * l - q;
        color.x = %s(p, q, h + 0.33333333333333331);
        color.y = %s(p, q, h);
        color.z = %s(p, q, h - 0.33333333333333331);
    }
}
@if (%s) {
    half off = -0.5 * %s + 0.5;
    color = %s * color + off;
}
color = clamp(color, 0.0, 1.0);
@if (%s) {
    color.xyz = sqrt(color.xyz);
}
%s = half4(color.xyz, 1) * inColor.w;
)SkSL",
                _sample896.c_str(), (_outer.linearize ? "true" : "false"),
                (_outer.grayscale ? "true" : "false"), (_outer.invertBrightness ? "true" : "false"),
                (_outer.invertLightness ? "true" : "false"), HSLToRGB_name.c_str(),
                HSLToRGB_name.c_str(), HSLToRGB_name.c_str(),
                (_outer.hasContrast ? "true" : "false"),
                args.fUniformHandler->getUniformCStr(contrastModVar),
                args.fUniformHandler->getUniformCStr(contrastModVar),
                (_outer.linearize ? "true" : "false"), args.fOutputColor);
    }

private:
    void onSetData(const GrGLSLProgramDataManager& pdman,
                   const GrFragmentProcessor& _proc) override {
        const GrHighContrastFilterEffect& _outer = _proc.cast<GrHighContrastFilterEffect>();
        { pdman.set1f(contrastModVar, (_outer.contrastMod)); }
    }
    UniformHandle contrastModVar;
};
GrGLSLFragmentProcessor* GrHighContrastFilterEffect::onCreateGLSLInstance() const {
    return new GrGLSLHighContrastFilterEffect();
}
void GrHighContrastFilterEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                       GrProcessorKeyBuilder* b) const {
    b->add32((uint32_t)hasContrast);
    b->add32((uint32_t)grayscale);
    b->add32((uint32_t)invertBrightness);
    b->add32((uint32_t)invertLightness);
    b->add32((uint32_t)linearize);
}
bool GrHighContrastFilterEffect::onIsEqual(const GrFragmentProcessor& other) const {
    const GrHighContrastFilterEffect& that = other.cast<GrHighContrastFilterEffect>();
    (void)that;
    if (contrastMod != that.contrastMod) return false;
    if (hasContrast != that.hasContrast) return false;
    if (grayscale != that.grayscale) return false;
    if (invertBrightness != that.invertBrightness) return false;
    if (invertLightness != that.invertLightness) return false;
    if (linearize != that.linearize) return false;
    return true;
}
GrHighContrastFilterEffect::GrHighContrastFilterEffect(const GrHighContrastFilterEffect& src)
        : INHERITED(kGrHighContrastFilterEffect_ClassID, src.optimizationFlags())
        , contrastMod(src.contrastMod)
        , hasContrast(src.hasContrast)
        , grayscale(src.grayscale)
        , invertBrightness(src.invertBrightness)
        , invertLightness(src.invertLightness)
        , linearize(src.linearize) {
    this->cloneAndRegisterAllChildProcessors(src);
}
std::unique_ptr<GrFragmentProcessor> GrHighContrastFilterEffect::clone() const {
    return std::make_unique<GrHighContrastFilterEffect>(*this);
}
#if GR_TEST_UTILS
SkString GrHighContrastFilterEffect::onDumpInfo() const {
    return SkStringPrintf(
            "(contrastMod=%f, hasContrast=%s, grayscale=%s, invertBrightness=%s, "
            "invertLightness=%s, linearize=%s)",
            contrastMod, (hasContrast ? "true" : "false"), (grayscale ? "true" : "false"),
            (invertBrightness ? "true" : "false"), (invertLightness ? "true" : "false"),
            (linearize ? "true" : "false"));
}
#endif
GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrHighContrastFilterEffect);
#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrHighContrastFilterEffect::TestCreate(
        GrProcessorTestData* d) {
    using InvertStyle = SkHighContrastConfig::InvertStyle;
    SkHighContrastConfig config{/*grayscale=*/d->fRandom->nextBool(),
                                InvertStyle(d->fRandom->nextRangeU(0, int(InvertStyle::kLast))),
                                /*contrast=*/d->fRandom->nextF()};
    return GrHighContrastFilterEffect::Make(d->inputFP(), config,
                                            /*linearize=*/d->fRandom->nextBool());
}
#endif
