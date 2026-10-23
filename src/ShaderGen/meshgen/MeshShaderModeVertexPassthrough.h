// MeshShaderModeVertexPassthrough.h — VertexPassthrough 模式 main() 体
//
// 跨步协作模型（Stride Loop）：
// 64 线程协作处理最多 192 顶点 64 三角形。
//
// 输出恒 triangle list：mesh shader 的图元拓扑由 layout 声明（triangles），
// Fan/TriangleStrip 是固定管线的装配规则（依赖连续顶点流）——mesh 的分组
// 模型下跨组必然错（扇心错位/组边界丢三角形）。
// 需要 fan/strip 语义的几何必须在 CPU 侧转成 triangle list。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <vulkan/vulkan.h>
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
        /// 本该模式的 varying 写入规则集（= 描述符里的那张表）。
        /// 发射器把它原样交给 EmitVaryingWrites —— 写什么由表决定，
        /// 模式下不再持有第二份语义清单。
        MeshVaryingWriteRuleSet varying_write_rules;
        /// 位置属性格式（仅 LineQuad 使用：vec2/vec3 端点读取适配）。
        /// 由 MeshTemplateEmitter 填充——放在上下文里，模式体发射器签名才能统一为
        /// (std::string&, const MeshShaderModeContext&)，无需签名适配包装层。
        VkFormat                position_format = VK_FORMAT_UNDEFINED;
    };

    inline void EmitVertexPassthroughBody(
        std::string &ms,
        const MeshShaderModeContext &ctx)
    {
        const auto &resolved_stage_interface = *ctx.stage_interface;
        const auto &varying_cfg = *ctx.varying_cfg;
        const std::string local_size = std::to_string(ctx.max_invocations);
        const std::string max_vertices = std::to_string(ctx.max_vertices);

        std::string varying_outputs;
        EmitVaryingWrites(
            varying_outputs,
            resolved_stage_interface,
            varying_cfg,
            ctx.varying_write_rules);

        std::string body = GetMeshShaderTemplate(
            GetMeshShaderModeTemplateFile(MeshShaderMode::VertexPassthrough));
        if (body.empty())
            ms += "#error mesh shader template missing: vertex_passthrough.glsl.tmpl\n";
        else
        {
            ApplyMeshTemplateSlot(body, "local_size", local_size);
            ApplyMeshTemplateSlot(body, "max_vertices", max_vertices);
            ApplyMeshTemplateSlot(body, "varying_outputs", varying_outputs);
            ms += body;
        }
    }
}

