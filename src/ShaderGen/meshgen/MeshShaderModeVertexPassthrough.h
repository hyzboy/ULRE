// MeshShaderModeVertexPassthrough.h — VertexPassthrough 模式 main() 体
//
// 每线程 1 顶点：位置变换 + varying 赋值，直通到 mesh 顶点槽。
//
// 输出恒 triangle list：mesh shader 的图元拓扑由 layout 声明（triangles），
// Fan/TriangleStrip 是固定管线的装配规则（依赖连续顶点流）——mesh 的分组
// 模型下跨组必然错（扇心错位/组边界丢三角形）。
// 需要 fan/strip 语义的几何必须在 CPU 侧转成 triangle list。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <string>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>

namespace hgl::graph::mtl
{
    // VertexPassthrough 模式共享上下文
    struct MeshShaderModeContext
    {
        const ValueArray<InterStageSemanticContractEntry> *stage_interface;
        uint32_t                max_invocations;
        uint32_t                max_vertices;
        const MaterialVertexVaryingConfig *varying_cfg;
        bool                    emit_world_pos;
        bool                    emit_world_normal;
    };

    inline void EmitVertexPassthroughBody(
        std::string &ms,
        const MeshShaderModeContext &ctx)
    {
        const auto &resolved_stage_interface = *ctx.stage_interface;
        const auto &varying_cfg = *ctx.varying_cfg;

        // 每线程 1 顶点：位置变换 + varying 赋值，直通到 mesh 顶点槽
        // 组内顶点槽位（0 .. max_vertices-1；全局顶点号由 VertexIndexID 宏处理）
        ms += "    const uint vid = gl_LocalInvocationIndex;\n";
        ms += "\n";

        ms += "    const uint total_vertices = pc_vertex_index.total_vertices;\n";
        // 本组有效顶点数（所有 invocation 相同值 → SetMeshOutputsEXT 一致；
        // groupCountX = ceil(total/group_size)，末组起始 <= total，不会 uint 下溢）
        ms += "    const uint verts_this_group = min(";
        ms += std::to_string(ctx.max_invocations);
        ms += "u, total_vertices - gl_WorkGroupID.x * ";
        ms += std::to_string(ctx.max_invocations);
        ms += "u);\n";
        // 图元数 = 顶点数/3（triangle list；group size 必须是 3 的倍数——见
        // MeshShaderAssembler 的 % 3 守卫，组内三角形永不跨组）
        ms += "    SetMeshOutputsEXT(verts_this_group, verts_this_group / 3u);\n";
        ms += "    if (vid >= verts_this_group)\n";
        ms += "        return;\n";
        ms += "\n";

        // 全局顶点号解析：非索引直通（绘制顺序 = 顶点号）或索引查表（is_indexed）。
        // 与 VS 的 s1_index 分支语义一致（mesh 无 gl_VertexIndex，用跨组全局序号）
        ms += "    MeshVertexIndex = gl_WorkGroupID.x * ";
        ms += std::to_string(ctx.max_invocations);
        ms += "u + gl_LocalInvocationIndex;\n";
        ms += "    if (pc_vertex_index.is_indexed != 0u)\n";
        ms += "        MeshVertexIndex = sbo_vertex_index.data[pc_vertex_index.index_base + MeshVertexIndex];\n";
        ms += "\n";

        // LoadVertexData（读 SSBO 单顶点；VertexIndexID 宏 = MeshVertexIndex）
        ms += "    LoadVertexData();\n";

        // 变换（对齐 VS：world pos/normal 一次 GetL2W + camera.vp 投影）
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::DataIndexID))
        {
            // 与 VS 一致：实例 → MaterialPrivateDataIndexRows 查表（材质数据槽——FS 用它查 mtl.data[].color 等）。
            // gl_InstanceIndex 宏 = first_instance + gl_WorkGroupID.y（跨 draw_batch 正确）
            // perprimitiveEXT：图元号 = vid/3（triangle list，每 3 顶点 1 图元）
            ms += "    fragDataIndexID[vid / 3u] = ResolveMaterialPrivateDataIndex(gl_InstanceIndex);\n";
        }
        if (varying_cfg.emit_vertex_color_from_palette)
            ms += "    fragVertexColor[vid] = unpackUnorm4x8(color_palette.color[ColorIndex]);\n";
        else if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::Color))
            ms += "    fragVertexColor[vid] = Color;\n";

        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::UV0))
            ms += "    fragUV0[vid] = TexCoord;\n";
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::Luminance))
            ms += "    fragLuminance[vid] = Luminance;\n";
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::FragDirection))
            ms += "    fragDirection[vid] = normalize(Position);\n";

        if (ctx.emit_world_pos || ctx.emit_world_normal)
        {
            ms += "    mat4 _l2w = GetL2W();\n";
            ms += "    vec4 _world_pos = _l2w * GetLocalPos();\n";
            if (ctx.emit_world_pos)
                ms += "    fragWorldPos[vid] = _world_pos.xyz;\n";
            if (ctx.emit_world_normal)
                ms += "    fragWorldNormal[vid] = normalize(mat3(_l2w) * Normal);\n";
            // world-normal 路径投影恒为 WorldCameraVP（与 VS 一致）
            ms += "    gl_MeshVerticesEXT[vid].gl_Position = camera.vp * _world_pos;\n";
        }
        else
        {
            ms += "    gl_MeshVerticesEXT[vid].gl_Position = GetClipPos(GetLocalPos());\n";
        }

        // 三角形索引（mesh 输出恒 triangle list——每 3 连续顶点 1 三角形，
        // vid%3==0 的线程写 (vid,vid+1,vid+2)；非 3 倍数顶点余数不构成三角形）
        ms += "    if ((vid % 3u) == 0u && (vid + 2u) < verts_this_group)\n";
        ms += "        gl_PrimitiveTriangleIndicesEXT[vid / 3u] = uvec3(vid, vid + 1u, vid + 2u);\n";
    }
}
