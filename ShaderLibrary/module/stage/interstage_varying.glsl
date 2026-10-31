// ShaderLibrary/module/stage/interstage_varying.glsl
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
//   HGL_FRAGMENT_INPUT_VARYINGS  → `in` 声明（片段阶段）
//
// ── 网格阶段前置条件 ──
//   1. `module/stage/stage_common.glsl` 已 include（提供 perprimitiveEXT 等）
//   2. `ULRE_MESH_MAX_VERTS` / `ULRE_MESH_MAX_PRIMITIVES` 已按拓扑字面量定义
//      （与 layout 行的 max_vertices / max_primitives 是同一份事实）
//   3. `generated/common_options.glsl` 已 include（提供 HGL_VARYING_*）
//   写出函数 `WriteMeshVertexOutputs()` 需要 `generated/mesh_options.glsl`
//   （顶点解码 / 位置映射 / 变换），因此它单独放在
//   `module/stage/interstage_varying_writer.glsl` 里 —— 声明 → options → 写出，
//   这个顺序是硬要求（见 module/stage/stage_body.glsl 的说明）。
//
// ── 片段阶段前置条件 ──
//   只有一条：主着色器在**所有 `#extension` 之后**（也就是所有 include 之后、
//   第一条非预处理 token 之前）定义 HGL_FRAGMENT_INPUT_VARYINGS 并 include 本文件。
//   开关源 `generated/common_options.glsl` 由片段段自己拉，主着色器不必知道它。
//
//   本文件只管**声明**，不管搬运。`fragXxx` → `SurfaceInput` 的赋值留在各领域
//   主着色器里，因为「搬过来的东西怎么解释」是领域语义而不是接口语义：
//   · `sky` 没有 world pos，它拿 `fragDirection` 当 `worldPos` / `viewDir` 用；
//   · `checker_3d` / `debug_normal` 的 `viewDir` 分别是 `-fragWorldPos` 与
//     归一化后的法线，`forward_lit` 却是 `camera.camera_world_pos - fragWorldPos`；
//   · 面法线兜底必须排在顶点法线之后，次序写死在 main 里。
//   这些差异不是重复，所以没有、也不要加统一的 `UnpackVaryings(...)`。
//
// 顶点色在网格阶段恒开（见 EmitInterStageVaryingOptions 的约束 2）：网格侧不区分
// 直出 / 调色板 / 缺省白三种来源，一律输出顶点色。多写一个片段侧不读的 varying
// 在 Vulkan 里是合法的（只浪费一点带宽），少写则不合法 —— 所以恒开是安全方向。
//
// 新增一条阶段间语义的顺序：ShaderSemanticRegistry.cpp 的注册表 → 本文件补
// 声明 → writer 补写出 → EmitInterStageVaryingOptions 的 emittable 判定放行，
// 并在消费该语义的领域主着色器里自行赋值。
// 四处缺一不可，且 location 数字**永远**只来自 interstage_locations.glsl。
#ifndef HGL_INTERSTAGE_VARYING_GLSL
#define HGL_INTERSTAGE_VARYING_GLSL

#include "contract/interstage_locations.glsl"

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
// 片段阶段：输入 varying
// ═══════════════════════════════════════════════════════════════════════════
#if defined(HGL_FRAGMENT_INPUT_VARYINGS)

// 片段段自己拉开关源：主着色器只需「定义 HGL_FRAGMENT_INPUT_VARYINGS + include
// 本文件」两行，不必知道 `generated/` 下有几个文件。网格段的开关源由形状主着色器
// 显式 include（它的 include 顺序是硬契约，声明必须在 topology layout 之后）。
#include "generated/common_options.glsl"

// ⚠ 声明**次序**是 SPIR-V 字节等价性的一部分（glslang 按声明序发射 OpVariable）。
// 这一段的次序刻意与迁移前各 `.frag` 里的手抄次序一致：DataIndexID → WorldPos →
// WorldNormal → UV0 → StyleID → Direction → Luminance → VertexColor。改动它会让
// 全部片段 SPV 变字节（语义不变），等价性比对与基线都会失效。
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
#if defined(HGL_VARYING_FRAG_DIRECTION)
layout(location=HGL_LOCATION_FRAG_DIRECTION) in vec3 fragDirection;
#endif
#if defined(HGL_VARYING_LUMINANCE)
layout(location=HGL_LOCATION_LUMINANCE) in float fragLuminance;
#endif
#if defined(HGL_VARYING_VERTEX_COLOR)
layout(location=HGL_LOCATION_VERTEX_COLOR) in vec4 fragVertexColor;
#endif

#endif // HGL_FRAGMENT_INPUT_VARYINGS

#endif // HGL_INTERSTAGE_VARYING_GLSL
