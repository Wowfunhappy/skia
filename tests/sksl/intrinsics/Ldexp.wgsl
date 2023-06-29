struct FSIn {
  @builtin(front_facing) sk_Clockwise: bool,
};
struct FSOut {
  @location(0) sk_FragColor: vec4<f32>,
};
struct _GlobalUniforms {
  a: f32,
};
@binding(0) @group(0) var<uniform> _globalUniforms: _GlobalUniforms;
var<private> b: i32;
fn main(_stageOut: ptr<function, FSOut>) {
  {
    let _skTemp0 = ldexp(_globalUniforms.a, b);
    (*_stageOut).sk_FragColor.x = f32(_skTemp0);
  }
}
@fragment fn fragmentMain(_stageIn: FSIn) -> FSOut {
  var _stageOut: FSOut;
  main(&_stageOut);
  return _stageOut;
}
