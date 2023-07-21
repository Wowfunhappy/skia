### Compilation failed:

error: :19:40 error: unresolved type 'sampler2D'
var<private> uTextureSampler_0_Stage1: sampler2D;
                                       ^^^^^^^^^


diagnostic(off, derivative_uniformity);
struct FSIn {
  @builtin(front_facing) sk_Clockwise: bool,
  @location(0) vLocalCoord_Stage0: vec2<f32>,
};
struct FSOut {
  @location(0) sk_FragColor: vec4<f32>,
};
struct uniformBuffer {
  sk_RTAdjust: vec4<f32>,
  uIncrement_Stage1_c0: vec2<f32>,
  uKernel_Stage1_c0: array<vec4<f32>, 7>,
  umatrix_Stage1_c0_c0: mat3x3<f32>,
  uborder_Stage1_c0_c0_c0: vec4<f32>,
  usubset_Stage1_c0_c0_c0: vec4<f32>,
  unorm_Stage1_c0_c0_c0: vec4<f32>,
};
@group(0) @binding(0) var<uniform> _uniforms : uniformBuffer;
var<private> uTextureSampler_0_Stage1: sampler2D;
fn MatrixEffect_Stage1_c0_c0_h4h4f2(_skParam0: vec4<f32>, _skParam1: vec2<f32>) -> vec4<f32> {
  let _input = _skParam0;
  let _coords = _skParam1;
  {
    var _1_inCoord: vec2<f32> = (_uniforms.umatrix_Stage1_c0_c0 * vec3<f32>(_coords, 1.0)).xy;
    _1_inCoord = _1_inCoord * _uniforms.unorm_Stage1_c0_c0_c0.xy;
    var _2_subsetCoord: vec2<f32>;
    _2_subsetCoord.x = _1_inCoord.x;
    _2_subsetCoord.y = _1_inCoord.y;
    var _3_clampedCoord: vec2<f32> = _2_subsetCoord;
    let _skTemp0 = sample(uTextureSampler_0_Stage1, _3_clampedCoord * _uniforms.unorm_Stage1_c0_c0_c0.zw);
    var _4_textureColor: vec4<f32> = _skTemp0;
    let _skTemp1 = floor(_1_inCoord.x + 0.001);
    var _5_snappedX: f32 = _skTemp1 + 0.5;
    if (_5_snappedX < _uniforms.usubset_Stage1_c0_c0_c0.x) || (_5_snappedX > _uniforms.usubset_Stage1_c0_c0_c0.z) {
      {
        _4_textureColor = _uniforms.uborder_Stage1_c0_c0_c0;
      }
    }
    return _4_textureColor;
  }
}
fn main(_stageIn: FSIn, _stageOut: ptr<function, FSOut>) {
  {
    var outputColor_Stage0: vec4<f32>;
    var outputCoverage_Stage0: vec4<f32>;
    {
      outputColor_Stage0 = vec4<f32>(1.0);
      outputCoverage_Stage0 = vec4<f32>(1.0);
    }
    var _6_output: vec4<f32> = vec4<f32>(0.0);
    var _7_coord: vec2<f32> = _stageIn.vLocalCoord_Stage0 - vec2<f32>(12.0 * _uniforms.uIncrement_Stage1_c0);
    var _8_coordSampled: vec2<f32> = vec2<f32>(0.0);
    _8_coordSampled = _7_coord;
    let _skTemp2 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp2 * _uniforms.uKernel_Stage1_c0[0].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp3 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp3 * _uniforms.uKernel_Stage1_c0[0].y;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp4 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp4 * _uniforms.uKernel_Stage1_c0[0].z;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp5 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp5 * _uniforms.uKernel_Stage1_c0[0].w;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp6 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp6 * _uniforms.uKernel_Stage1_c0[1].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp7 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp7 * _uniforms.uKernel_Stage1_c0[1].y;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp8 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp8 * _uniforms.uKernel_Stage1_c0[1].z;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp9 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp9 * _uniforms.uKernel_Stage1_c0[1].w;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp10 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp10 * _uniforms.uKernel_Stage1_c0[2].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp11 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp11 * _uniforms.uKernel_Stage1_c0[2].y;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp12 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp12 * _uniforms.uKernel_Stage1_c0[2].z;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp13 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp13 * _uniforms.uKernel_Stage1_c0[2].w;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp14 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp14 * _uniforms.uKernel_Stage1_c0[3].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp15 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp15 * _uniforms.uKernel_Stage1_c0[3].y;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp16 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp16 * _uniforms.uKernel_Stage1_c0[3].z;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp17 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp17 * _uniforms.uKernel_Stage1_c0[3].w;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp18 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp18 * _uniforms.uKernel_Stage1_c0[4].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp19 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp19 * _uniforms.uKernel_Stage1_c0[4].y;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp20 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp20 * _uniforms.uKernel_Stage1_c0[4].z;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp21 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp21 * _uniforms.uKernel_Stage1_c0[4].w;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp22 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp22 * _uniforms.uKernel_Stage1_c0[5].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp23 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp23 * _uniforms.uKernel_Stage1_c0[5].y;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp24 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp24 * _uniforms.uKernel_Stage1_c0[5].z;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp25 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp25 * _uniforms.uKernel_Stage1_c0[5].w;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _8_coordSampled = _7_coord;
    let _skTemp26 = MatrixEffect_Stage1_c0_c0_h4h4f2(outputColor_Stage0, _8_coordSampled);
    _6_output = _6_output + _skTemp26 * _uniforms.uKernel_Stage1_c0[6].x;
    _7_coord = _7_coord + vec2<f32>(_uniforms.uIncrement_Stage1_c0);
    _6_output = _6_output * outputColor_Stage0;
    var output_Stage1: vec4<f32> = _6_output;
    {
      (*_stageOut).sk_FragColor = output_Stage1 * outputCoverage_Stage0;
    }
  }
}
@fragment fn fragmentMain(_stageIn: FSIn) -> FSOut {
  var _stageOut: FSOut;
  main(_stageIn, &_stageOut);
  return _stageOut;
}

1 error
