// MeshShaderModeLineQuad.h — LineQuad 模式 main() 体
//
// 每线程 1 线段 → 4 顶点 2 三角形（line-to-quad 展开）。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <vulkan/vulkan.h>
#include <string>
#include <hgl/mtl/MaterialVertexVaryingConfig.h>
#include "MeshShaderModeVertexPassthrough.h"   // MeshShaderModeContext
#include "MeshShaderTemplate.h"

namespace hgl::graph::mtl
{
    inline void EmitLineQuadBody(
        std::string &ms,
        const MeshShaderModeContext &ctx,
        VkFormat position_format)
    {
        const auto &resolved_stage_interface = *ctx.stage_interface;
        const auto &varying_cfg = *ctx.varying_cfg;

        // 每线程 1 线段 → 4 顶点 2 三角形（line-to-quad）
        const std::string group_size = std::to_string(ctx.max_invocations);

        // ── 静态主体（S3：外移至 ShaderLibrary/mesh/line_quad.glsl.tmpl）──
        // 模板内容 = 原 C++ 内嵌文本，逐字节一致；两个槽位：
        //   {{group_size}}    线程组大小（3 处）
        //   {{endpoint_read}} 端点读取——按 position_format 适配：
        //                     vec2（2D 平面材质）→ vec3 构造；vec3 直读
        // 槽紧贴下一行且不自带换行，填入文本自带结尾 \n（保证与旧输出等价）。
        // 条件性 varying 写入仍由下方 C++ 按 stage interface / varying_cfg 决定后追加。
        const char *const endpoint_read =
            position_format == VK_FORMAT_R32G32_SFLOAT
                ? "    const vec3 from = vec3(sbo_vertex_position.data[v0], 0.0);\n"
                  "    const vec3 to   = vec3(sbo_vertex_position.data[v1], 0.0);\n"
                : "    const vec3 from = sbo_vertex_position.data[v0];\n"
                  "    const vec3 to   = sbo_vertex_position.data[v1];\n";

        std::string body = GetMeshShaderTemplate("line_quad.glsl.tmpl");
        if (body.empty())
            ms += "#error mesh shader template missing: line_quad.glsl.tmpl\n";
        else
        {
            ApplyMeshTemplateSlot(body, "group_size", group_size);
            ApplyMeshTemplateSlot(body, "endpoint_read", endpoint_read);
            ms += body;
        }

        // varying（per-vertex；per-primitive 语义按图元号——每线段 2 图元共享）
        EmitVaryingWrites(
            ms,
            resolved_stage_interface,
            varying_cfg,
            MeshVaryingIndexModel::LineQuad);
    }
}
