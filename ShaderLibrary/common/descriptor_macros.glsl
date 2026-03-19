// descriptor_macros.glsl — 标准描述符集/绑定宏定义
//
// 默认值对应 3D 标准布局（Scene=0, Transform=1, Material=2, VertexData=3）。
// 2D 生成器或自定义材质可在 #include 之前 #define 覆盖默认值。
//
// Resort() 按字母序在 set 内排列 binding，set 间按 Scene < Transform < Material < VertexData
// 空 set 被压缩：如 2D 的 NDC 模式无 Scene set，Transform 降为 set=0。

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

#ifndef TID_SET
#define TID_SET TRANSFORM_SET
#endif

#ifndef TID_BINDING
#if defined(MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR) && (MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR)
#define TID_BINDING 2
#else
#define TID_BINDING 1
#endif
#endif

#ifndef MID_SET
#define MID_SET TRANSFORM_SET
#endif

#ifndef MID_BINDING
#define MID_BINDING 1
#endif

// ── Scene set (Resort 字母序: camera < sky < viewport) ──
// 当 sky 不存在时: camera=0, viewport=1 → 在 shader 中 #define VIEWPORT_BINDING 1

#ifndef CAMERA_BINDING
#define CAMERA_BINDING 0
#endif

#ifndef SKY_BINDING
#define SKY_BINDING 1
#endif

#ifndef VIEWPORT_BINDING
#define VIEWPORT_BINDING 2
#endif

// ── VertexData set (SSBO 顶点获取) ──

#ifndef VTX_DATA_BINDING
#define VTX_DATA_BINDING 18
#endif

#ifndef IDX_DATA_BINDING
#define IDX_DATA_BINDING 19
#endif

#endif // DESCRIPTOR_MACROS_GLSL
