/// ShaderLayoutBuilder.cpp
///
/// Builds ShaderLayoutContract from a MaterialCreateInfo after Resort() has run.
/// Three buckets are filled:
///   1. vertex_locations  — from VIAArray (ShaderCreateInfoVertex::GetInput)
///   2. descriptor_sets   — one entry per non-empty ShaderDescriptorSet
///   3. descriptor_bindings — one entry per individual ShaderDescriptor

#include <hgl/shadergen/ShaderLayoutBuilder.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include <hgl/common/VertexInputDef.h>
#include <hgl/mtl/DescriptorBindingContract.h>
#include <hgl/mtl/SamplerName.h>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace hgl::graph
{

namespace
{
constexpr int RESERVED_VERTEX_DATA_SET = int(DESCRIPTOR_SET_TYPE_COUNT);
constexpr int RESERVED_VTX_DATA_BINDING = 18;
constexpr int RESERVED_IDX_DATA_BINDING = 19;

void AddLayoutEntryIfMissing(std::vector<ShaderLayoutEntry> &entries, const char *macro_name, const int value)
{
    if (!macro_name || !*macro_name || value < 0)
        return;

    const auto found = std::find_if(entries.begin(), entries.end(),
        [macro_name](const ShaderLayoutEntry &entry)
        {
            return entry.macro_name == macro_name;
        });

    if (found == entries.end())
        entries.push_back({ macro_name, value });
}

std::string BuildMacroName(const char *name, const char *suffix)
{
    if (!name || !*name || !suffix || !*suffix)
        return {};

    std::string result;
    result.reserve(std::strlen(name) + std::strlen(suffix));

    for (const char *p = name; *p; ++p)
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));

    result += suffix;
    return result;
}

const std::array<std::string, DESCRIPTOR_SET_TYPE_COUNT> &GetDescriptorSetMacroNameCache()
{
    static const std::array<std::string, DESCRIPTOR_SET_TYPE_COUNT> cache = []
    {
        std::array<std::string, DESCRIPTOR_SET_TYPE_COUNT> names{};

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; ++i)
            names[i] = BuildMacroName(DescriptSetTypeName[i], "_SET");

        return names;
    }();

    return cache;
}
}

// ─────────────────────────────────────────────────────────────────────────────
// Naming helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string GetVertexAttribLocationMacroName(VertexAttrib attrib)
{
    return BuildMacroName(GetVertexAttribName(attrib), "_LOCATION");
}

const char *GetDescriptorSetMacroName(DescriptorSetType set_type)
{
    if (set_type == DescriptorSetType::Unknow)
        return nullptr;

    const size_t index = size_t(set_type);
    if (index >= DESCRIPTOR_SET_TYPE_COUNT)
        return nullptr;

    return GetDescriptorSetMacroNameCache()[index].c_str();
}

std::string GetDescriptorBindingMacroName(const char *descriptor_name)
{
    if (!descriptor_name || !*descriptor_name)
        return {};

    if (const char *macro_name = mtl::FindDescriptorBindingMacroNameByDescriptorName(descriptor_name))
        return macro_name;

    mtl::SamplerName::SamplerSlot slot;
    if (mtl::SamplerName::TryGetSlotFromDescriptorName(descriptor_name, slot))
        return mtl::SamplerName::ToBindingMacroName(slot);

    return BuildMacroName(descriptor_name, "_BINDING");
}

static void AppendDescriptorBindingMacros(ShaderLayoutContract &contract,
                                         const char           *descriptor_name,
                                         const int             binding)
{
    if (!descriptor_name || !*descriptor_name || binding < 0)
        return;

    const std::string canonical_macro = GetDescriptorBindingMacroName(descriptor_name);
    if (!canonical_macro.empty())
        AddLayoutEntryIfMissing(contract.descriptor_bindings, canonical_macro.c_str(), binding);

    // Keep legacy short aliases for older shaders that still reference TEX_* names.
    mtl::SamplerName::SamplerSlot slot;
    if (!mtl::SamplerName::TryGetSlotFromDescriptorName(descriptor_name, slot))
        return;

    const size_t slot_index = size_t(slot);
    if (slot_index >= mtl::SamplerName::SamplerSlotCount)
        return;

    const auto &binding_macro_cache = mtl::SamplerName::GetBindingMacroNameCache();
    AddLayoutEntryIfMissing(contract.descriptor_bindings,
                            binding_macro_cache[slot_index].c_str(),
                            binding);
}

// ─────────────────────────────────────────────────────────────────────────────
// Builder
// ─────────────────────────────────────────────────────────────────────────────

ShaderLayoutContract BuildShaderLayoutContract(const mtl::MaterialCreateInfo &mci)
{
    ShaderLayoutContract contract;

    // ── Bucket 1: vertex input locations ─────────────────────────────────────
    const ShaderCreateInfoVertex *vsc = mci.GetVertexShader();
    if (vsc)
    {
        const VIAArray &via_array = vsc->GetInput();
        for (uint i = 0; i < via_array.count; ++i)
        {
            const VIA &via = via_array.items[i];
            const std::string macro = GetVertexAttribLocationMacroName(via.attrib);
            if (!macro.empty())
                contract.vertex_locations.push_back({ macro, static_cast<int>(via.location) });
        }
    }

    // ── Bucket 2 & 3: descriptor sets and bindings ───────────────────────────
    const ShaderDescriptorSetArray &set_array = mci.GetDescriptorInfo().Get();

    for (size_t si = 0; si < DESCRIPTOR_SET_TYPE_COUNT; ++si)
    {
        const ShaderDescriptorSet &sd_set = set_array[si];
        if (sd_set.count <= 0)
            continue;

        // Descriptor set number
        const char *set_macro = GetDescriptorSetMacroName(DescriptorSetType(si));
        if (set_macro)
            contract.descriptor_sets.push_back({ set_macro, sd_set.set });

        // Individual descriptor bindings within this set
        for (auto &kv : sd_set.descriptor_map)
        {
            const ShaderDescriptor *sd = kv.second;
            if (!sd || sd->binding < 0)
                continue;

            AppendDescriptorBindingMacros(contract, sd->name, sd->binding);
        }
    }

    // Vertex fetch SSBO uses a reserved descriptor set/binding range outside the
    // material descriptor list; emit these here so GLSL no longer carries numbers.
    AddLayoutEntryIfMissing(contract.descriptor_sets, "VERTEX_DATA_SET", RESERVED_VERTEX_DATA_SET);
    AddLayoutEntryIfMissing(contract.descriptor_bindings, "VTX_DATA_BINDING", RESERVED_VTX_DATA_BINDING);
    AddLayoutEntryIfMissing(contract.descriptor_bindings, "IDX_DATA_BINDING", RESERVED_IDX_DATA_BINDING);

    // ── Sort each bucket in ascending value order ─────────────────────────────
    auto by_value = [](const ShaderLayoutEntry &a, const ShaderLayoutEntry &b)
    {
        return a.value < b.value;
    };

    std::sort(contract.vertex_locations.begin(),    contract.vertex_locations.end(),    by_value);
    std::sort(contract.descriptor_sets.begin(),     contract.descriptor_sets.end(),     by_value);
    std::sort(contract.descriptor_bindings.begin(), contract.descriptor_bindings.end(), by_value);

    return contract;
}

}  // namespace hgl::graph
