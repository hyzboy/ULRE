// @ulre begin
// @ulre name l2w_ssbo
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// l2w_ssbo.glsl — LocalToWorld 变换 SSBO 统一声明
//
// 需要 L2W_SET / L2W_BINDING 宏（descriptor_macros.glsl 提供默认值）
//
// 用法:
//   #include "common/l2w_ssbo.glsl"
//   L2W_SSBO;
//   ...
//   mat4 m = l2w.mats[TransformID];

#ifndef L2W_SSBO_GLSL
#define L2W_SSBO_GLSL

#define L2W_SSBO \
    layout(set=L2W_SET, binding=L2W_BINDING) readonly buffer LocalToWorldData { \
        mat4 mats[]; \
    } l2w

#endif // L2W_SSBO_GLSL
