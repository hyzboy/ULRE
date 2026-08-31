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

        // ── UV varying ───────────────────────────────────────────
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::UV0))
        {
            ms += "    fragUV0[base_vid + 0u] = vec2(rot_tl_u, rot_tl_v);  // TL\n";
            ms += "    fragUV0[base_vid + 1u] = vec2(rot_bl_u, rot_bl_v);  // BL\n";
            ms += "    fragUV0[base_vid + 2u] = vec2(rot_tr_u, rot_tr_v);  // TR\n";
            ms += "    fragUV0[base_vid + 3u] = vec2(rot_br_u, rot_br_v);  // BR\n";
        }

        // ── 颜色 varying ─────────────────────────────────────────
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::Color))
        {
            ms += "    for (int i = 0; i < 4; i++)\n";
            ms += "        fragVertexColor[base_vid + uint(i)] = char_color;\n";
        }

        // ── DataIndexID varying（perprimitiveEXT——每字符 2 图元各写 1 份）──
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::DataIndexID))
        {
            ms += "    const uint data_id = ResolveMaterialPrivateDataIndex(gl_DrawID);\n";
            ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 0u] = data_id;\n";
            ms += "    fragDataIndexID[gl_LocalInvocationIndex * 2u + 1u] = data_id;\n";
        }

        // StyleID varying（perprimitiveEXT——flat 每图元样式索引 → FS 查 sbo_char_style）
        if (FindMaterialStageInterfaceEntry(resolved_stage_interface, InterStageSemantic::StyleID))
        {
            ms += "    fragStyleID[gl_LocalInvocationIndex * 2u + 0u] = style_id;\n";
            ms += "    fragStyleID[gl_LocalInvocationIndex * 2u + 1u] = style_id;\n";
        }
    }
}
