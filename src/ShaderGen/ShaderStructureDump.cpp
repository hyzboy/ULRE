/// ShaderStructureDump.cpp — 编译产物结构化快照（S4 试点实现）

#include <hgl/mtl/ShaderStructureDump.h>
#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/mtl/DescriptorSetLayoutAllocator.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <algorithm>
#include <vector>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

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

    // ── 资源（结构 + 解出的 set/binding）──────────────────────────────────────
    const ShaderResourceSchema &schema = ctx.GetShaderResourceSchema();
    const DescriptorSetLayoutAllocator &alloc = ctx.GetDescriptorAllocator();

    struct Row
    {
        int         set = -1;
        int         binding = -1;
        std::string text;
    };

    std::vector<Row> rows;
    rows.reserve(schema.resources.size());

    for (const ShaderResourceSlot &res : schema.resources)
    {
        // 解出的 set/binding 取自分配器（文本断言检查的正是这两个数）
        int set = -1;
        int binding = -1;

        if (res.semantic_layer == DescriptorSemanticLayer::UBO)
        {
            if (const UBODescriptor *ubo = alloc.GetUBO(res.name))
            {
                set = ubo->set;
                binding = ubo->binding;
            }
        }
        else if (res.semantic_layer == DescriptorSemanticLayer::SSBO)
        {
            if (const SSBODescriptor *ssbo = alloc.GetSSBO(res.name))
            {
                set = ssbo->set;
                binding = ssbo->binding;
            }
        }

        Row row;
        row.set = set;
        row.binding = binding;

        // 未在材质分配器中命中的两种情形要区分开：
        //   Scene 全局集——按帧绑定，per-material 不注册（正常，非缺失）
        //   其余——确实未解出（异常，值得注意）
        const std::string set_text = set >= 0
            ? std::to_string(set)
            : (res.set_type == DescriptorSetType::Scene ? "global" : "unallocated");
        const std::string binding_text = binding >= 0
            ? std::to_string(binding)
            : (res.set_type == DescriptorSetType::Scene ? "global" : "unallocated");

        row.text = "resource";
        AppendKV(row.text, "semantic", GetDescriptorSemanticName(res.semantic));
        AppendKV(row.text, "layer", GetDescriptorSemanticLayerName(res.semantic_layer));
        AppendKV(row.text, "declared_set", GetDescriptorSetTypeName(res.set_type));
        AppendKV(row.text, "set", set_text);
        AppendKV(row.text, "binding", binding_text);
        AppendKV(row.text, "stages", StageBitsText(res.stage_flags));
        AppendKV(row.text, "ssbo_type", GetSSBOTypeName(res.ssbo_type));
        AppendKV(row.text, "name", res.name);
        AppendKV(row.text, "struct", res.struct_name);
        AppendKV(row.text, "required", res.required ? 1 : 0);
        AppendKV(row.text, "fallback", res.allow_fallback ? 1 : 0);

        rows.push_back(std::move(row));
    }

    // 顺序稳定化：(set, binding, 行文本) —— 不依赖内部容器顺序
    std::sort(rows.begin(), rows.end(),
        [](const Row &a, const Row &b)
        {
            if (a.set != b.set)
                return a.set < b.set;
            if (a.binding != b.binding)
                return a.binding < b.binding;
            return a.text < b.text;
        });

    out += "resource_count=";
    out += std::to_string(rows.size());
    out += '\n';

    for (const Row &row : rows)
    {
        out += row.text;
        out += '\n';
    }

    return out;
}

}//namespace hgl::graph::mtl
