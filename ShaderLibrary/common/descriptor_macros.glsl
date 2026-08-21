// @ulre begin
// @ulre name descriptor_macros
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// descriptor_macros.glsl — 标准描述符集/绑定宏定义
//
// 默认值对应 3D 标准布局（Scene=0, PerObject=1, Material=2, Bindless=3）。
// 2D 生成器或自定义材质可在 #include 之前 #define 覆盖默认值。
//
// 固定布局：set 间按 Scene(0) < PerObject(1) < Material(2) < Bindless(3)。
// 行表 SSBO 声明（mtl_data_index_rows / mtl_texture_layer_rows / l2w_index_rows）
// 不在此定义默认值：由 CompileCompositorMaterial 依据 descriptor_info 统一生成并
// 注入（buffer 声明 + Resolve 函数，不再写死在 .glsl）。
// 材质实例 mtl SSBO 的 struct/buffer 声明同样由 CompileCompositorMaterial 统一生成并注入。

#ifndef DESCRIPTOR_MACROS_GLSL
#define DESCRIPTOR_MACROS_GLSL

// ── Descriptor Set 索引 ──

#ifndef SCENE_SET
#define SCENE_SET 0
#endif

#ifndef PER_OBJECT_SET
#define PER_OBJECT_SET 1
#endif

#ifndef MATERIAL_SET
#define MATERIAL_SET 2
#endif

// ── PerObject set ──

#ifndef L2W_SET
#define L2W_SET PER_OBJECT_SET
#endif

// ── 顶点数据 SSBO（MeshShader 方向：顶点输入统一为 SSBO）──
// s1_position_vec3 / s1_uv / s1_ntb / s1_joint 模块使用
#ifndef VERTEX_SET
#define VERTEX_SET PER_OBJECT_SET
#endif
#ifndef VERTEX_POSITION_BINDING
#define VERTEX_POSITION_BINDING 4
#endif
#ifndef VERTEX_UV_BINDING
#define VERTEX_UV_BINDING 5
#endif
#ifndef VERTEX_NTB_BINDING
#define VERTEX_NTB_BINDING 6
#endif
#ifndef VERTEX_JOINT_BINDING
#define VERTEX_JOINT_BINDING 7
#endif
#ifndef VERTEX_INDEX_BINDING
#define VERTEX_INDEX_BINDING 8
#endif

#ifndef VERTEX_COLOR_BINDING
#define VERTEX_COLOR_BINDING 9
#endif

#ifndef VERTEX_LUMINANCE_BINDING
#define VERTEX_LUMINANCE_BINDING 10
#endif

#ifndef L2W_BINDING
#define L2W_BINDING 0
#endif

// ── Scene set ──

#ifndef CAMERA_BINDING
#define CAMERA_BINDING 0
#endif

#ifndef SKY_BINDING
#define SKY_BINDING 1
#endif

#ifndef VIEWPORT_BINDING
#define VIEWPORT_BINDING 2
#endif

#ifndef COLOR_PALETTE_BINDING
#define COLOR_PALETTE_BINDING 3
#endif

#ifndef BINDLESS_SET
#define BINDLESS_SET 3
#endif

#endif // DESCRIPTOR_MACROS_GLSL
