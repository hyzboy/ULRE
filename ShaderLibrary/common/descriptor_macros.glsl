// descriptor_macros.glsl — 标准描述符集/绑定宏定义
//
// 默认值对应 3D 标准布局（Scene=0, Transform=1, Material=2, VertexData=3）。
// 2D 生成器或自定义材质可在 #include 之前 #define 覆盖默认值。
//
// 固定布局：set 间按 Scene(0) < Transform(1) < Material(2) < VertexData(3)。
// Material 内行表绑定（mtl_data_index_rows / mtl_texture_layer_rows）不在此定义默认值：
// 由 CompileCompositorMaterial 依据 descriptor_info 注入 MI_DATA_INDEX_ROWS_SET/BINDING、
// MI_TEXTURE_LAYER_ROWS_SET/BINDING 宏（声明统一生成，不再写死在 .glsl）。
// 材质实例 mtl SSBO 的 struct/buffer 声明同样由 CompileCompositorMaterial 统一生成并注入。

#ifndef DESCRIPTOR_MACROS_GLSL
#define DESCRIPTOR_MACROS_GLSL

// ── Descriptor Set 索引 ──

#ifndef SCENE_SET
#define SCENE_SET 0
#endif

#ifndef TRANSFORM_SET
#define TRANSFORM_SET 1
#endif

#ifndef MATERIAL_SET
#define MATERIAL_SET 2
#endif

#ifndef VERTEX_DATA_SET
#define VERTEX_DATA_SET 3
#endif

// ── Transform set ──

#ifndef L2W_SET
#define L2W_SET TRANSFORM_SET
#endif

#ifndef L2W_BINDING
#define L2W_BINDING 0
#endif

#ifndef L2W_INDEX_ROWS_SET
#define L2W_INDEX_ROWS_SET TRANSFORM_SET
#endif

#ifndef L2W_INDEX_ROWS_BINDING
#define L2W_INDEX_ROWS_BINDING 1
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

#ifndef BINDLESS_SET
#define BINDLESS_SET 4
#endif

// ── VertexData set (SSBO 顶点获取) ──

#ifndef VTX_DATA_BINDING
#define VTX_DATA_BINDING 18
#endif

#ifndef IDX_DATA_BINDING
#define IDX_DATA_BINDING 19
#endif

#endif // DESCRIPTOR_MACROS_GLSL
