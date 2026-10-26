// @ulre begin
// @ulre name s1_stream_defaults
// @ulre kind Utility
// @ulre priority 5
// @ulre end
// Stage 1: 顶点流装载的缺省实现（空对象模式）。
//
// 各数据流模块（s1_uv / s1_ntb / s1_color / …）自述一个 `void LoadXxx()`；
// 位置模块（s1_position_*）的 `LoadVertexData()` 无条件按固定顺序调用它们。
// 未装载某数据流时，本文件用同名空函数补齐，因此：
//   - 位置模块里不再出现 `#ifdef HGL_*_LOADER`（"漏加一个 #ifdef = 属性静默为 0"
//     这一历史事故类别被物理消除，而不是靠校验工具兜底）；
//   - include 顺序不再需要"位置模块必须最后"的隐式约定——依赖的是函数声明，
//     不是由"谁先 define 了宏"决定的预处理期开关。
//
// 由位置模块 `#include`，依赖各数据流模块的 `S1_*_GLSL` 守卫判断在位与否。
#ifndef S1_STREAM_DEFAULTS_GLSL
#define S1_STREAM_DEFAULTS_GLSL

// UV0（s1_uv / s1_uv_rg16f 提供）
#ifndef S1_STREAM_UV
void LoadUV() { }
#endif

// 法线（s1_ntb / s1_ntb_rg8 / s1_ntb_rg16f / s1_ntb_a2bgr10 提供）
#ifndef S1_STREAM_NTB
void LoadNTB() { }
#endif

#endif // S1_STREAM_DEFAULTS_GLSL
