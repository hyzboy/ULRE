// MeshShaderModeCharQuad.h — CharQuad 模式 SSBO 声明 + main() 体
//
// 每线程 1 字符实例 → 6 顶点 2 三角形（字符 quad）。
// 三层数据模型：CharInfo + CharStyle + CharInstance。

#pragma once

#include <hgl/mtl/MaterialStageInterface.h>
#include <string>
#include "MeshShaderModeVertexPassthrough.h"   // MeshShaderModeContext
#include "MeshShaderTemplate.h"

namespace hgl::graph::mtl
{
    // CharQuad SSBO 声明（全局作用域，void main 之前）
    // 结构体真源 = ShaderLibrary/vertex/s1_text_char_quad.glsl（GLSL 模块，
    // 与 CPU 侧 TextCharSSBO.h 布局逐字段对应，见该文件的 static_assert）
    inline void EmitCharQuadSSBODeclarations(std::string &ms)
    {
        ms += "// ── Text CharQuad SSBOs（结构真源：ShaderLibrary/vertex/s1_text_char_quad.glsl）──\n";
        ms += "#include \"vertex/s1_text_char_quad.glsl\"\n";
        ms += "\n";
    }

    // CharQuad main() 体
    inline void EmitCharQuadBody(
        std::string &ms,
        const MeshShaderModeContext &ctx)
    {
        const auto &resolved_stage_interface = *ctx.stage_interface;

        // 每线程 1 字符实例 → 4 顶点 2 三角形（字符 quad，顶点复用——TR/BL 共享）
        // 三层数据模型：CharInfo + CharStyle + CharInstance
        const std::string group_size = std::to_string(ctx.max_invocations);

        // ── 静态主体（S3：外移至 ShaderLibrary/mesh/char_quad.glsl.tmpl）──
        // 模板内容 = 原 C++ 内嵌文本，逐字节一致；唯一槽位 {{group_size}}。
        // 条件性 varying 写入仍由下方 C++ 按 stage interface 决定后追加。
        std::string body = GetMeshShaderTemplate("char_quad.glsl.tmpl");
        if (body.empty())
            ms += "#error mesh shader template missing: char_quad.glsl.tmpl\n";
        else
        {
            ApplyMeshTemplateSlot(body, "group_size", group_size);
            ms += body;
        }

        MaterialVertexVaryingConfig dummy_varying_cfg{};
        EmitVaryingWrites(
            ms,
            resolved_stage_interface,
            dummy_varying_cfg,
            MeshVaryingIndexModel::CharQuad);
    }
}
