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
#include "MeshShaderTemplate.h"
#include "MeshShaderVaryingGen.h"

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
        const std::string group_size = std::to_string(ctx.max_invocations);

        std::string varying_outputs;
        EmitVaryingWrites(
            varying_outputs,
            resolved_stage_interface,
            varying_cfg,
            MeshVaryingIndexModel::PerVertex,
            ctx.emit_world_pos,
            ctx.emit_world_normal);

        std::string body = GetMeshShaderTemplate("vertex_passthrough.glsl.tmpl");
        if (body.empty())
            ms += "#error mesh shader template missing: vertex_passthrough.glsl.tmpl\n";
        else
        {
            ApplyMeshTemplateSlot(body, "group_size", group_size);
            ApplyMeshTemplateSlot(body, "varying_outputs", varying_outputs);
            ms += body;
        }
    }
}

