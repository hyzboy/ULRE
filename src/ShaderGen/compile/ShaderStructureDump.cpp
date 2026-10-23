/// ShaderStructureDump.cpp — 编译产物结构化快照（S4 试点实现）

#include <hgl/mtl/ShaderStructureDump.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <algorithm>
#include <vector>

namespace hgl::graph::mtl
{
namespace
{
    void AppendKV(std::string &out, const char *key, const char *value)
    {
        out += ' ';
        out += key;
        out += '=';
        out += (value && value[0]) ? value : "-";
    }

    void AppendKV(std::string &out, const char *key, const std::string &value)
    {
        AppendKV(out, key, value.empty() ? "-" : value.c_str());
    }

    void AppendKV(std::string &out, const char *key, const long long value)
    {
        out += ' ';
        out += key;
        out += '=';
        out += std::to_string(value);
    }

    /// stage_flags → 稳定的可读位串（不用十六进制数值：位含义比数值更耐改）
    std::string StageBitsText(const uint32_t stage_flags)
    {
        std::string text;

        const auto add = [&](const ShaderStage stage, const char *name)
        {
            if (stage_flags & uint32_t(stage))
            {
                if (!text.empty())
                    text += '|';
                text += name;
            }
        };

        add(ShaderStage::Mesh,     "Mesh");
        add(ShaderStage::Fragment, "Fragment");

        // 出现非 Mesh/Fragment 位时显式标出（架构上不应发生——mesh 是唯一顶点路径）
        const uint32_t known = uint32_t(ShaderStage::Mesh) | uint32_t(ShaderStage::Fragment);
        if (stage_flags & ~known)
        {
            if (!text.empty())
                text += '|';
            text += "OTHER";
        }

        return text.empty() ? std::string("-") : text;
    }
}//namespace

std::string DumpShaderStructure(const ShaderBuildContext &ctx, const char *label)
{
    std::string out;

    out += "material ";
    out += (label && label[0]) ? label : "<unnamed>";
    out += '\n';

    // ── stage 存在性（不含 GLSL 文本——文本断言正是本快照要取代的东西）──
    const ShaderCreateInfo *mesh = ctx.GetStageShader(ShaderStage::Mesh);
    const ShaderCreateInfo *frag = ctx.GetStageShader(ShaderStage::Fragment);

    out += "stage mesh";
    AppendKV(out, "present", mesh ? 1 : 0);
    AppendKV(out, "glsl_nonempty", (mesh && !mesh->GetFinalGLSL().empty()) ? 1 : 0);
    out += '\n';

    out += "stage fragment";
    AppendKV(out, "present", frag ? 1 : 0);
    AppendKV(out, "glsl_nonempty", (frag && !frag->GetFinalGLSL().empty()) ? 1 : 0);
    out += '\n';

    // ── program identity 的**存在性**（不含 hash 值：值随文本变动，写 golden 无意义）──
    out += "program_link";
    AppendKV(out, "present", ctx.HasProgramLink() ? 1 : 0);
    if (ctx.HasProgramLink())
    {
        const ShaderLinkSpec &link = ctx.GetProgramLink();
        AppendKV(out, "valid", link.IsValid() ? 1 : 0);
        AppendKV(out, "resource_layout_hash_set", link.resource_layout_hash != 0 ? 1 : 0);
        AppendKV(out, "vertex_input_hash_set", link.vertex_input_hash != 0 ? 1 : 0);
        AppendKV(out, "render_target_hash_set", link.render_target_hash != 0 ? 1 : 0);
        AppendKV(out, "compiler_hash_set", link.compiler_hash != 0 ? 1 : 0);
    }
    out += '\n';

    // ── 求解层状态：模块列表（依赖序）与有效 varying 配置 ────────────────────
    const std::vector<std::string> &module_names = ctx.GetResolvedModules();
    out += "modules";
    AppendKV(out, "count", static_cast<long long>(module_names.size()));
    out += '\n';
    for (const std::string &name : module_names)
    {
        out += "module";
        AppendKV(out, "name", name);
        out += '\n';
    }

    if (const MaterialVertexVaryingConfig *varying = ctx.GetEffectiveVarying())
    {
        out += "varying";
        AppendKV(out, "data_index_id", varying->emit_data_index_id ? 1 : 0);
        AppendKV(out, "vertex_color", varying->emit_vertex_color ? 1 : 0);
        AppendKV(out, "uv0", varying->emit_uv0 ? 1 : 0);
        AppendKV(out, "world_pos", varying->emit_world_pos ? 1 : 0);
        AppendKV(out, "world_normal", varying->emit_world_normal ? 1 : 0);
        AppendKV(out, "luminance", varying->emit_luminance ? 1 : 0);
        AppendKV(out, "frag_direction", varying->emit_frag_direction ? 1 : 0);
        AppendKV(out, "transform_id_attr", varying->use_transform_id_attr ? 1 : 0);
        AppendKV(out, "palette_color", varying->emit_vertex_color_from_palette ? 1 : 0);
        AppendKV(out, "style_id", varying->emit_style_id ? 1 : 0);
        out += '\n';
    }

    // ── 资源需求（2026-10-11 S1：条目载体已删，只剩活信号）──────────────
    // 旧资源表恒为空（唯一生产者已删）⇒ 表与 set/binding 列一并移除；
    // 快照改为直报编译期直判信号。
    out += "requires_recipe_runtime_rows=";
    out += ctx.RequiresRecipeRuntimeRows() ? "1" : "0";
    out += '\n';
    // 两类行需求是两个独立信号，诊断快照必须都给出：只报合并信号会让
    // 「纯纹理材质被当成要 payload」这类回归在快照里看不出来。
    out += "requires_recipe_payload_row=";
    out += ctx.RequiresRecipePayloadRow() ? "1" : "0";
    out += '\n';

    return out;
}

}//namespace hgl::graph::mtl
