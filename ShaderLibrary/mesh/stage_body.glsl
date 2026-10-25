// ShaderLibrary/mesh/stage_body.glsl
//
// VertexPassthrough 网格阶段的**物理固定**主体：组内顶点装载 → 顶点输出 → 三角形索引。
// 形状主着色器（mesh/shape/tri_*.mesh）只声明输出 varying 与一个写出函数，主体在这里。
//
// 主着色器在 include 本文件**之前**必须完成：
//   1. `#include "mesh/stage_common_head.glsl"` + 拓扑字面量 + `#include "mesh/stage_common.glsl"`
//   2. 声明本形状的输出 varying 数组
//   3. `#include "generated/mesh_options.glsl"`
//      —— 提供 `LoadVertexData()` / `GetLocalPos()` / `GetClipPos()` /
//         `GetWorldPos()` / `GetWorldNormal()` 与各顶点属性变量
//   4. 定义 `void WriteMeshVertexOutputs(uint vid)`
//   5. `#define ULRE_MESH_MAX_VERTS <本组顶点数>` 与 `#define ULRE_MESH_INVOCATIONS <local_size_x>`
//
// 为什么 2/3 的顺序不能换：s1_* 数据模块声明属性变量，s1_position_* 用
// `HGL_*_LOADER` 宏把它们折进 `LoadVertexData()`，因此 options 必须整体排在
// varying 声明之后、写出函数之前。
//
// local_size_x / max_vertices / max_primitives 是 CPU 侧派发契约（每组 64 线程、
// 每组 192 顶点 = 64 三角形），不是可运行时调节的选项 —— 它们必须以**字面量**
// 写在主着色器的 layout 行里（ValidateMeshSourceTopology 逐字解析）。
//
// 本文件不得含结构性 #if 分支（include guard 除外）。
#ifndef ULRE_MESH_STAGE_BODY_GLSL
#define ULRE_MESH_STAGE_BODY_GLSL

void main()
{
    uint geometry_id = MeshDrawCommandsRef(pc_root.addr_batch_mesh_draw_params).cmds[gl_DrawID].geometry_id;
    draw_params = MeshDrawParamsRef(global_addresses.addr_mesh_draw_params_pool).rows[geometry_id];
    draw_params.first_instance = MeshDrawCommandsRef(pc_root.addr_batch_mesh_draw_params).cmds[gl_DrawID].first_instance;

    // 元素数：索引路径取 index_count、非索引路径取 vertex_count（旧 total_vertices 把两者混装）
    const uint total_vertices = (draw_params.is_indexed != 0u)
        ? draw_params.index_count
        : draw_params.vertex_count;
    const uint max_group_verts = ULRE_MESH_MAX_VERTS;
    const uint group_base_vertex = gl_WorkGroupID.x * max_group_verts;
    const uint raw_verts = (group_base_vertex < total_vertices)
        ? min(max_group_verts, total_vertices - group_base_vertex)
        : 0u;
    const uint prims_this_group = raw_verts / 3u;
    const uint verts_this_group = prims_this_group * 3u;

    SetMeshOutputsEXT(verts_this_group, prims_this_group);
    if (verts_this_group == 0u)
        return;

    for (uint vid = gl_LocalInvocationIndex; vid < verts_this_group; vid += ULRE_MESH_INVOCATIONS)
    {
        MeshVertexIndex = group_base_vertex + vid;
        if (draw_params.is_indexed != 0u)
            MeshVertexIndex = sbo_vertex_index.data[draw_params.index_base + MeshVertexIndex];

        LoadVertexData();
        WriteMeshVertexOutputs(vid);
        gl_MeshVerticesEXT[vid].gl_Position = GetClipPos(GetLocalPos());
    }

    for (uint p = gl_LocalInvocationIndex; p < prims_this_group; p += ULRE_MESH_INVOCATIONS)
    {
        const uint bvid = p * 3u;
        gl_PrimitiveTriangleIndicesEXT[p] = uvec3(bvid, bvid + 1u, bvid + 2u);
    }
}

#endif // ULRE_MESH_STAGE_BODY_GLSL
