// ShaderLibrary/module/stage/interstage_varying_writer.glsl
//
// 网格阶段的**标准** varying 写出函数：`WriteMeshVertexOutputs(uint vid)`。
//
// 之所以从 `module/stage/interstage_varying.glsl` 拆出来：写出表达式依赖
// `generated/mesh_options.glsl` 提供的 `GetWorldPos()` / `GetWorldNormal()` /
// `TexCoord` / `Luminance` / `Position`，而 options 必须排在 varying 声明之后
//（属性变量先于写出函数可见，见 module/stage/stage_body.glsl）。两个文件拼起来才是
// 「声明 → options → 写出」这个固定顺序。
//
// 只有三角形直通形状（拓扑 `triangles` + options 提供整条顶点链路）用本文件。
// 其它拓扑自带几何主体，写出钩子的签名也不同，钩子留在各自形状 master 里
// （`linequad_256.mesh` / `charquad_256.mesh`）：
//   · `module/stage/line_quad_body.glsl`  → `WriteLineQuadVertexOutputs(uint base_vid)`
//   · `module/stage/char_quad_body.glsl`  → `WriteCharQuadVertexOutputs(...)` + `WriteCharQuadPrimitiveOutputs(...)`
// 「套不进按 vid 逐点写」的只是**函数形状**：一线段 / 一字符一次出 4 个顶点，
// 图元级 varying 还要写两个槽。它们同样按 `HGL_VARYING_*` 开关写，读的是与声明
// 同一批宏 —— 差别只在循环写几次，不再是「按 varying 集合另抄一份主着色器」。
//
// 前置条件：`module/stage/interstage_varying.glsl`（声明）+ `generated/mesh_options.glsl`
// 都已 include，且 `HGL_MESH_OUTPUT_VARYINGS` 已定义。
#ifndef HGL_INTERSTAGE_VARYING_WRITER_GLSL
#define HGL_INTERSTAGE_VARYING_WRITER_GLSL

#if !defined(HGL_MESH_OUTPUT_VARYINGS)
#error "interstage_varying_writer.glsl: 只能用于网格阶段（先定义 HGL_MESH_OUTPUT_VARYINGS）"
#endif
#if !defined(ULRE_MESH_MAX_VERTS) || !defined(ULRE_MESH_MAX_PRIMITIVES)
#error "interstage_varying_writer.glsl: 拓扑维度 ULRE_MESH_MAX_VERTS / ULRE_MESH_MAX_PRIMITIVES 未定义"
#endif

// 每条语义的写入表达式就是它在顶点流里的定义，没有任何按形状的分支：
// 形状差异曾经只体现在「写哪几条」上，那正是这次合并要消掉的东西。
void WriteMeshVertexOutputs(uint vid)
{
#if defined(HGL_VARYING_DATA_INDEX_ID)
    // 图元级（Flat）：同一三角形的 3 个顶点写同一个槽，重复写值相同。
    fragDataIndexID[vid / 3u] = gl_InstanceIndex;
#endif
#if defined(HGL_VARYING_WORLD_POS)
    fragWorldPos[vid] = GetWorldPos(GetLocalPos());
#endif
#if defined(HGL_VARYING_WORLD_NORMAL)
    fragWorldNormal[vid] = GetWorldNormal(Normal);
#endif
#if defined(HGL_VARYING_UV0)
    fragUV0[vid] = TexCoord;
#endif
#if defined(HGL_VARYING_VERTEX_COLOR)
    fragVertexColor[vid] = GetVertexColor();
#endif
#if defined(HGL_VARYING_FRAG_DIRECTION)
    // 天空盒方向：本形状的位置流就是单位方向向量。
    fragDirection[vid] = normalize(Position);
#endif
#if defined(HGL_VARYING_LUMINANCE)
    fragLuminance[vid] = Luminance;
#endif
}

#endif // HGL_INTERSTAGE_VARYING_WRITER_GLSL
