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

std::string GetDescriptorBindingMacroName(const ShaderDescriptor *sd)
{
    if (!sd || !sd->name || !*sd->name)
        return {};

    // Phase F: Enum-only lookup — no reverse parsing from descriptor name
    
    // For SSBO/UBO: lookup by semantic
    if (sd->semantic != mtl::DescriptorSemantic::Unknown)
    {
        const auto &meta = mtl::GetDescriptorSemanticMeta(sd->semantic);
        if (meta.binding_macro_name && *meta.binding_macro_name)
            return meta.binding_macro_name;
    }

    // For Texture/TextureSampler: lookup by slot (if available)
    const auto *tex_sd = dynamic_cast<const TextureDescriptor *>(sd);
    if (tex_sd && tex_sd->slot != mtl::SamplerSlot::Count)
        return mtl::ToBindingMacroName(tex_sd->slot);

    const auto *tex_sampler_sd = dynamic_cast<const TextureSamplerDescriptor *>(sd);
    if (tex_sampler_sd && tex_sampler_sd->slot != mtl::SamplerSlot::Count)
        return mtl::ToBindingMacroName(tex_sampler_sd->slot);

    // Fallback: use descriptor name directly (for custom descriptors)
    return BuildMacroName(sd->name, "_BINDING");
}

static void AppendDescriptorBindingMacros(ShaderLayoutContract &contract,
                                         const ShaderDescriptor *sd)
{
    if (!sd || sd->binding < 0)
        return;

    // Phase F: Emit macro based on semantic/slot, not reverse name lookup
    const std::string canonical_macro = GetDescriptorBindingMacroName(sd);
    if (!canonical_macro.empty())
        AddLayoutEntryIfMissing(contract.descriptor_bindings, canonical_macro.c_str(), sd->binding);

    // Keep legacy short aliases for older shaders that still reference TEX_* names.
    // (Only for texture descriptors with known slots)
    const auto *tex_sd = dynamic_cast<const TextureSamplerDescriptor *>(sd);
    if (!tex_sd || tex_sd->slot == mtl::SamplerSlot::Count)
        return;

    const size_t slot_index = size_t(tex_sd->slot);
    if (slot_index >= mtl::SamplerSlotCount)
        return;

    const auto &binding_macro_cache = mtl::GetSamplerBindingMacroNameCache();
    AddLayoutEntryIfMissing(contract.descriptor_bindings,
                            binding_macro_cache[slot_index].c_str(),
                            sd->binding);
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

            AppendDescriptorBindingMacros(contract, sd);
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
