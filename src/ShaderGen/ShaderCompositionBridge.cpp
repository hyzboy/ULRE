#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderLogic.h>

#include <cstring>
#include <string>
#include <vector>

namespace hgl::graph::mtl {

static bool CStrEq(const char *lhs, const char *rhs)
{
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

static const FixedDescriptorEntry *FindDescriptorByName(
    const ComposedMaterialDef &def,
    const char *name)
{
    if (!name || !*name)
        return nullptr;

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const auto &entry = def.descriptor_entries[i];
        if (CStrEq(entry.name, name) || CStrEq(entry.struct_name, name))
            return &entry;
    }

    return nullptr;
}

static bool ContainsName(const std::vector<std::string> &names, const char *name)
{
    if (!name || !*name)
        return false;

    for (const auto &item : names)
    {
        if (item == name)
            return true;
    }

    return false;
}

static void AppendUniqueName(std::vector<std::string> &names, const char *name)
{
    if (!name || !*name)
        return;

    if (!ContainsName(names, name))
        names.emplace_back(name);
}

static void CollectRequiredNamesFromLogicBlock(
    const ShaderLogicBlock &block,
    std::vector<std::string> &required_resources,
    std::vector<std::string> &required_helpers)
{
    if (block.required_resources && block.required_resource_count > 0)
    {
        for (uint32_t i = 0; i < block.required_resource_count; ++i)
            AppendUniqueName(required_resources, block.required_resources[i]);
    }

    if (block.required_helpers && block.required_helper_count > 0)
    {
        for (uint32_t i = 0; i < block.required_helper_count; ++i)
            AppendUniqueName(required_helpers, block.required_helpers[i]);
    }
}

bool BuildComposedMaterialDefFromLogic(
    const ComposedMaterialDef &base_def,
    const MaterialLogicDef &logic,
    ComposedMaterialBuildFromLogicResult &out)
{
    out.filtered_descriptors.clear();
    out.diagnostics.missing_resources.clear();
    out.def.logic_required_helpers.clear();

    std::vector<std::string> required_resources;
    std::vector<std::string> required_helpers;
    CollectRequiredNamesFromLogicBlock(logic.vertex, required_resources, required_helpers);
    CollectRequiredNamesFromLogicBlock(logic.fragment, required_resources, required_helpers);

    out.def = base_def;

    out.vertex_business = base_def.vertex_business ? *base_def.vertex_business : VertexShaderBusiness{nullptr};
    out.fragment_business = base_def.fragment_business ? *base_def.fragment_business : FragmentShaderBusiness{nullptr};

    if (logic.vertex.main_logic)
        out.vertex_business.code = logic.vertex.main_logic;

    if (logic.fragment.main_logic)
        out.fragment_business.code = logic.fragment.main_logic;

    out.def.vertex_business = &out.vertex_business;
    out.def.fragment_business = &out.fragment_business;

    for (const auto &required_name : required_resources)
    {
        const FixedDescriptorEntry *entry = FindDescriptorByName(base_def, required_name.c_str());
        if (entry)
        {
            out.filtered_descriptors.push_back(*entry);
        }
        else
        {
            out.diagnostics.missing_resources.emplace_back(required_name);
        }
    }

    if (!out.filtered_descriptors.empty())
    {
        out.def.descriptor_entries = out.filtered_descriptors.data();
        out.def.descriptor_entry_count = uint32_t(out.filtered_descriptors.size());
    }
    else if (!required_resources.empty())
    {
        out.def.descriptor_entries = nullptr;
        out.def.descriptor_entry_count = 0;
    }

    out.def.logic_required_helpers = required_helpers;

    return out.diagnostics.missing_resources.empty();
}

} // namespace hgl::graph::mtl
