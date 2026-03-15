// material_instance_ssbo.glsl — 材质实例 SSBO 统一声明
//
// 使用前必须先定义 struct MaterialInstance { ... };
// 需要 MI_SET / MI_BINDING 宏（descriptor_macros.glsl 提供默认值）
//
// 用法:
//   struct MaterialInstance { vec4 Color; };
//   MI_SSBO;

#ifndef MATERIAL_INSTANCE_SSBO_GLSL
#define MATERIAL_INSTANCE_SSBO_GLSL

#ifndef MI_SET
#define MI_SET MATERIAL_SET
#endif

#ifndef MI_BINDING
#define MI_BINDING 0
#endif

#define MI_SSBO \
    layout(set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData { \
        MaterialInstance mi[]; \
    } mtl

#define MI_SSBO_SCALAR \
    layout(scalar, set=MI_SET, binding=MI_BINDING) readonly buffer MaterialInstanceData { \
        MaterialInstance mi[]; \
    } mtl

#endif // MATERIAL_INSTANCE_SSBO_GLSL
