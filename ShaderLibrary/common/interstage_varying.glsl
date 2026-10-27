// ShaderLibrary/common/interstage_varying.glsl
//
// 网格阶段 ↔ 片段阶段之间**唯一**的 varying 接口定义。
//
// location / 类型 / 插值限定符全部来自这里：不再由 C++ 按 varying 集合拼形状名、
// 挑一份「形状主着色器」，也不在每份 `.frag` 里手抄一遍 `layout(in)`。
//
// 开关来自 `generated/common_options.glsl` 的 `HGL_VARYING_*` 宏（产地：
// inc/hgl/mtl/InterStageVaryingOptions.h::EmitInterStageVaryingOptions）。
// 置位的语义在**两端同时**出现，未置位的在两端同时消失 ⇒ 接口天然 100% 匹配，
// 不可能再出现「多一个 / 少一个 location」的管线创建期不匹配。
//
// ── 本文件必须被两个 stage 各 include 一次，include 前必须定义其一 ──
//   HGL_MESH_OUTPUT_VARYINGS     → `out` 数组声明（网格阶段）
//   HGL_FRAGMENT_INPUT_VARYINGS  → `in` 声明 + UnpackVaryingsToSurfaceInput()（片段阶段）
//
// ── 网格阶段前置条件 ──
//   1. `mesh/stage_common.glsl` 已 include（提供 perprimitiveEXT 等）
//   2. `ULRE_MESH_MAX_VERTS` / `ULRE_MESH_MAX_PRIMITIVES` 已按拓扑字面量定义
//      （与 layout 行的 max_vertices / max_primitives 是同一份事实）
//   3. `generated/common_options.glsl` 已 include（提供 HGL_VARYING_*）
//   写出函数 `WriteMeshVertexOutputs()` 需要 `generated/mesh_options.glsl`
//   （顶点解码 / 位置映射 / 变换），因此它单独放在
//   `common/interstage_varying_writer.glsl` 里 —— 声明 → options → 写出，
//   这个顺序是硬要求（见 mesh/stage_body.glsl 的说明）。
//
// ── 片段阶段前置条件 ──
//   1. `generated/common_options.glsl` 已 include
//   其余（`common/surface_interface.glsl`）由本文件自己拉。
//
// 顶点色在网格阶段恒开（见 EmitInterStageVaryingOptions 的约束 2）：网格侧不区分
// 直出 / 调色板 / 缺省白三种来源，一律输出顶点色。多写一个片段侧不读的 varying
// 在 Vulkan 里是合法的（只浪费一点带宽），少写则不合法 —— 所以恒开是安全方向。
//
// 新增一条阶段间语义的顺序：ShaderSemanticRegistry.cpp 的注册表 → 本文件补
// 声明与解包 → writer 补写出 → EmitInterStageVaryingOptions 的 emittable 判定放行。
// 四处缺一不可，且 location 数字**永远**只来自 interstage_locations.glsl。
#ifndef HGL_INTERSTAGE_VARYING_GLSL
#define HGL_INTERSTAGE_VARYING_GLSL

#include "common/interstage_locations.glsl"

#if defined(HGL_MESH_OUTPUT_VARYINGS) && defined(HGL_FRAGMENT_INPUT_VARYINGS)
#error "interstage_varying.glsl: HGL_MESH_OUTPUT_VARYINGS 与 HGL_FRAGMENT_INPUT_VARYINGS 不能同时定义"
#endif
#if !defined(HGL_MESH_OUTPUT_VARYINGS) && !defined(HGL_FRAGMENT_INPUT_VARYINGS)
#error "interstage_varying.glsl: include 前必须定义 HGL_MESH_OUTPUT_VARYINGS 或 HGL_FRAGMENT_INPUT_VARYINGS"
#endif

// ═══════════════════════════════════════════════════════════════════════════
// 网格阶段：输出 varying 数组
// ═══════════════════════════════════════════════════════════════════════════
//
// 数组维数是拓扑契约：逐顶点语义按 max_vertices、Flat（perprimitiveEXT）语义
// 按 max_primitives。维数写错不会编译失败（数组越界在 GLSL 里是未定义行为），
// 所以它由 ShaderResourceSchemaRegressionGate 的固定网格主着色器审计守护。
#if defined(HGL_MESH_OUTPUT_VARYINGS)

#if defined(HGL_VARYING_DATA_INDEX_ID)
layout(location=HGL_LOCATION_DATA_INDEX_ID) perprimitiveEXT out uint fragDataIndexID[ULRE_MESH_MAX_PRIMITIVES];
#endif
#if defined(HGL_VARYING_WORLD_POS)
layout(location=HGL_LOCATION_WORLD_POS) out vec3 fragWorldPos[ULRE_MESH_MAX_VERTS];
#endif
#if defined(HGL_VARYING_WORLD_NORMAL)
layout(location=HGL_LOCATION_WORLD_NORMAL) out vec3 fragWorldNormal[ULRE_MESH_MAX_VERTS];
#endif
#if defined(HGL_VARYING_UV0)
layout(location=HGL_LOCATION_UV0) out vec2 fragUV0[ULRE_MESH_MAX_VERTS];
#endif
#if defined(HGL_VARYING_STYLE_ID)
layout(location=HGL_LOCATION_STYLE_ID) perprimitiveEXT out uint fragStyleID[ULRE_MESH_MAX_PRIMITIVES];
#endif
#if defined(HGL_VARYING_VERTEX_COLOR)
layout(location=HGL_LOCATION_VERTEX_COLOR) out vec4 fragVertexColor[ULRE_MESH_MAX_VERTS];
#endif
#if defined(HGL_VARYING_FRAG_DIRECTION)
layout(location=HGL_LOCATION_FRAG_DIRECTION) out vec3 fragDirection[ULRE_MESH_MAX_VERTS];
#endif
#if defined(HGL_VARYING_LUMINANCE)
layout(location=HGL_LOCATION_LUMINANCE) out float fragLuminance[ULRE_MESH_MAX_VERTS];
#endif

#endif // HGL_MESH_OUTPUT_VARYINGS

// ═══════════════════════════════════════════════════════════════════════════
// 片段阶段：输入 varying + 解包到 SurfaceInput
// ═══════════════════════════════════════════════════════════════════════════
#if defined(HGL_FRAGMENT_INPUT_VARYINGS)

#include "common/surface_interface.glsl"

#if defined(HGL_VARYING_DATA_INDEX_ID)
layout(location=HGL_LOCATION_DATA_INDEX_ID) flat perprimitiveEXT in  uint fragDataIndexID;
#endif
#if defined(HGL_VARYING_WORLD_POS)
layout(location=HGL_LOCATION_WORLD_POS) in vec3 fragWorldPos;
#endif
#if defined(HGL_VARYING_WORLD_NORMAL)
layout(location=HGL_LOCATION_WORLD_NORMAL) in vec3 fragWorldNormal;
#endif
#if defined(HGL_VARYING_UV0)
layout(location=HGL_LOCATION_UV0) in vec2 fragUV0;
#endif
#if defined(HGL_VARYING_STYLE_ID)
layout(location=HGL_LOCATION_STYLE_ID) flat perprimitiveEXT in  uint fragStyleID;
#endif
#if defined(HGL_VARYING_VERTEX_COLOR)
layout(location=HGL_LOCATION_VERTEX_COLOR) in vec4 fragVertexColor;
#endif
#if defined(HGL_VARYING_FRAG_DIRECTION)
layout(location=HGL_LOCATION_FRAG_DIRECTION) in vec3 fragDirection;
#endif
#if defined(HGL_VARYING_LUMINANCE)
layout(location=HGL_LOCATION_LUMINANCE) in float fragLuminance;
#endif

// 只把「阶段间传过来的东西」搬进 SurfaceInput：点位 / 法线 / UV / 顶点色 /
// 亮度 / 样式索引。**派生量不属于这里**（`worldPos` 的视线方向要用
// `camera.camera_world_pos` 还是 `-worldPos` 是领域自己决定的，`screenPos`
// 恒取 gl_FragCoord），面法线兜底同理留在主着色器里 —— 它们依赖该领域的
// `HGL_MATERIAL_FACE_NORMAL_FALLBACK` 与赋值次序（兜底必须排在顶点法线之后）。
//
// 未置位的语义保持 `si` 的初值不动，所以主着色器**必须**在调用前给 `si`
// 赋全套默认值 —— 这是有意的：默认值的语义（法线朝 +Z、视线朝 +Z、
// 亮度 1.0）是 `SurfaceInput` 契约的一部分，不能让解包函数替它隐式决定。
void UnpackVaryingsToSurfaceInput(inout SurfaceInput si)
{
#if defined(HGL_VARYING_VERTEX_COLOR)
    si.vertexColor = fragVertexColor;
#endif
#if defined(HGL_VARYING_WORLD_POS)
    si.worldPos = fragWorldPos;
#endif
#if defined(HGL_VARYING_WORLD_NORMAL)
    si.worldNormal = normalize(fragWorldNormal);
#endif
#if defined(HGL_VARYING_UV0)
    si.uv0 = fragUV0;
#endif
#if defined(HGL_VARYING_LUMINANCE)
    si.luminance = fragLuminance;
#endif
#if defined(HGL_VARYING_STYLE_ID)
    si.styleID = fragStyleID;
#endif
}

#endif // HGL_FRAGMENT_INPUT_VARYINGS

#endif // HGL_INTERSTAGE_VARYING_GLSL
