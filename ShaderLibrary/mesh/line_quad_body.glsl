// ShaderLibrary/mesh/line_quad_body.glsl
//
// 网格阶段主体・线段 → 四边形（LineQuad 拓扑）。
//
// 本文件只做**形状不变**的那部分：每条线段读两个端点、投到裁剪空间、按屏幕空间
// 法线把线段撑成 2 个三角形。顶点解码（s1_*）与局部→裁剪变换（s2_* / s3_*）
// 全部来自 `generated/mesh_options.glsl`，所以同一个主体同时服务
// 「3D + Passthrough3D + camera.vp」与「2D + NDCLift + l2w_only」两种映射家族：
// 主体只调 `GetClipPos(GetLocalPos())`，不自己拼变换。
//
// varying 的写入由主着色器提供的符号钩子 `WriteLineQuadVertexOutputs()` 负责
// （形状差异 = varying 集合，与主体无关）。
//
// 前置条件（由 `mesh/stage_common.glsl` + options 提供）：
//   MeshVertexIndex / VertexIndexID / draw_params / sbo_vertex_index / ViewportUBO
//   LoadVertexData() / GetLocalPos() / GetClipPos() / GetLineSize()

// 线宽缺省：LineQuad 几何主体无条件调用 `GetLineSize()`（.x 满宽 / .y 最细阈值）。
// 当顶点输入未装载 `vertex/s1_size.glsl`（未定义 S1_SIZE_GLSL）时，本文件
// 自持 1 像素默认线宽，不污染通用 mesh_options.glsl 或 3D 网格。
#ifndef S1_SIZE_GLSL
vec2 GetLineSize()
{
    return vec2(1.0, 1.0);
}
#endif

void main()
{
    const MeshDrawCommand cmd =
        MeshDrawCommandsRef(pc_root.addr_batch_mesh_draw_params).cmds[gl_DrawID];
    draw_params = MeshDrawParamsRef(global_addresses.addr_mesh_draw_params_pool)
                      .rows[cmd.geometry_id];
    draw_params.first_instance = cmd.first_instance;

    const uint line_id = gl_WorkGroupID.x * gl_WorkGroupSize.x
                       + gl_LocalInvocationIndex;
    const uint base_vid = gl_LocalInvocationIndex * 4u;

    // 元素数：索引路径取 index_count、非索引路径取 vertex_count —— 与 vertex_passthrough 同口径。
    // 【坑，2026-10-02 实测】直接读 vertex_count>>1 时，对「8 顶点 / 24 索引」的盒线框
    // （12 条线）会算成 4 条线（少 3 倍）⇒ 屏幕上只画出 4 条直线的 rectangle。
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
    SetMeshOutputsEXT(lines_this_group * 4u, lines_this_group * 2u);

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

    // ── 端点 A ──
    MeshVertexIndex = v0;
    LoadVertexData();
    const vec4 clip_from = GetClipPos(GetLocalPos());
    // 线宽取 A 端：V2F = [满宽(最粗), 最细阈值]，两端写的是同一份值。
    const vec2 line_size = GetLineSize();
    WriteLineQuadVertexOutputs(base_vid);

    // ── 端点 B ──
    MeshVertexIndex = v1;
    LoadVertexData();
    const vec4 clip_to = GetClipPos(GetLocalPos());

    const vec2 ndc_from = clip_from.xy / clip_from.w;
    const vec2 ndc_to   = clip_to.xy / clip_to.w;
    vec2 dir_ndc = ndc_to - ndc_from;
    if (length(dir_ndc) < 1e-6)
        dir_ndc = vec2(1.0, 0.0);   // 线段投影为点（朝向相机）——退化为水平方向
    dir_ndc = normalize(dir_ndc);
    const vec2 n_ndc = vec2(-dir_ndc.y, dir_ndc.x);

    const float width = line_size.x;      // 满宽（最粗，用户指定）
    const float min_width = line_size.y;  // 最细阈值（深度越大越细，clamp 到此下限）
    const float depth = 0.5 * (clip_from.w + clip_to.w);
    const float width_eff = clamp(width * (10.0 / max(depth, 1e-4)), min_width, width);
    const vec2 offset_ndc = n_ndc * (width_eff / float(viewport.viewport_resolution.y));

    vec4 c0 = clip_from; c0.xy += offset_ndc * clip_from.w;
    vec4 c1 = clip_from; c1.xy -= offset_ndc * clip_from.w;
    vec4 c2 = clip_to;   c2.xy += offset_ndc * clip_to.w;
    vec4 c3 = clip_to;   c3.xy -= offset_ndc * clip_to.w;

    gl_MeshVerticesEXT[base_vid + 0u].gl_Position = c0;
    gl_MeshVerticesEXT[base_vid + 1u].gl_Position = c1;
    gl_MeshVerticesEXT[base_vid + 2u].gl_Position = c2;
    gl_MeshVerticesEXT[base_vid + 3u].gl_Position = c3;

    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 0u] =
        uvec3(base_vid + 0u, base_vid + 1u, base_vid + 2u);
    gl_PrimitiveTriangleIndicesEXT[gl_LocalInvocationIndex * 2u + 1u] =
        uvec3(base_vid + 1u, base_vid + 3u, base_vid + 2u);
}
