// ShaderLibrary/module/stage/line_list_body.glsl
//
// 网格阶段主体・线段 → 原生线图元（LineList 拓扑）。
//
// 与 line_quad_body.glsl 同源，但**不做屏幕空间撑宽**：每条输入线段直接发射
// 1 个 `layout(lines)` 图元（2 顶点 1 line），光栅化与经典 LINE_LIST 逐像素
// 一致，线宽恒 1px（Vulkan lineWidth 动态状态未启用）。
//
// 顶点解码（s1_*）与局部→裁剪变换（s2_* / s3_*）同样全部来自
// `generated/mesh_options.glsl`：主体只调 `GetClipPos(GetLocalPos())`。
//
// varying 的写入由主着色器提供的符号钩子 `WriteLineListVertexOutputs()` 负责；
// 钩子与统一声明读同一批 `HGL_VARYING_*` options，本主体不关心具体接口集合。
//
// 前置条件（由 `module/stage/stage_common.glsl` + options 提供）：
//   MeshVertexIndex / VertexIndexID / draw_params / sbo_vertex_index
//   LoadVertexData() / GetLocalPos() / GetClipPos()

void main()
{
    const MeshDrawCommand cmd =
        MeshDrawCommandsRef(pc_root.addr_batch_mesh_draw_params).cmds[gl_DrawID];
    draw_params = MeshDrawParamsRef(global_addresses.addr_mesh_draw_params_pool)
                      .rows[cmd.geometry_id];
    draw_params.first_instance = cmd.first_instance;

    const uint line_id = gl_WorkGroupID.x * gl_WorkGroupSize.x
                       + gl_LocalInvocationIndex;
    const uint base_vid = gl_LocalInvocationIndex * 2u;

    // 元素数：索引路径取 index_count、非索引路径取 vertex_count —— 与 vertex_passthrough 同口径。
    const uint total_elements = (draw_params.is_indexed != 0u)
        ? draw_params.index_count
        : draw_params.vertex_count;
    const uint total_lines = total_elements >> 1u;   // 线段路径：每个元素 2 个端点

    // ⚠ `total_lines - x*group` 是**无符号**减法：过派发（X 偏大）时下溢成巨值 ⇒ min()
    // 取满 ⇒ 声明了满额输出却没写顶点（未定义数据）。这里用**饱和**求本组条数。
    // 保持单次 SetMeshOutputsEXT 调用 —— 实测「越界即 SetMeshOutputsEXT(0,0)+return」
    // 在本机 AMDVLK 上会挂死并丢设备，勿改成那种形态。
    const uint lines_before = gl_WorkGroupID.x * gl_WorkGroupSize.x;
    const uint lines_this_group = (lines_before >= total_lines)
        ? 0u
        : min(gl_WorkGroupSize.x, total_lines - lines_before);
    SetMeshOutputsEXT(lines_this_group * 2u, lines_this_group);

    if (line_id >= total_lines)
        return;

    uint v0 = draw_params.vertex_base + line_id * 2u;
    uint v1 = v0 + 1u;
    if (draw_params.is_indexed != 0u)
    {
        const uint i0 = draw_params.index_base + line_id * 2u;
        v0 = draw_params.vertex_base + sbo_vertex_index.data[i0];
        v1 = draw_params.vertex_base + sbo_vertex_index.data[i0 + 1u];
    }

    // ── 端点 A ──（实例属性取自 A：与 LineQuad 同口径，B 端只提供位置）
    MeshVertexIndex = v0;
    LoadVertexData();
    gl_MeshVerticesEXT[base_vid + 0u].gl_Position = GetClipPos(GetLocalPos());
    WriteLineListVertexOutputs(base_vid);

    // ── 端点 B ──
    MeshVertexIndex = v1;
    LoadVertexData();
    gl_MeshVerticesEXT[base_vid + 1u].gl_Position = GetClipPos(GetLocalPos());

    gl_PrimitiveLineIndicesEXT[gl_LocalInvocationIndex] =
        uvec2(base_vid, base_vid + 1u);
}
