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
#include <algorithm>
#include <cctype>
#include <cstring>

namespace hgl::graph
{

// ─────────────────────────────────────────────────────────────────────────────
// Naming helpers
// ─────────────────────────────────────────────────────────────────────────────

const char *GetVertexAttribLocationMacroName(VertexAttrib attrib)
{
    switch (attrib)
    {
    case VertexAttrib::Position:            return "POSITION_LOCATION";
    case VertexAttrib::Normal:              return "NORMAL_LOCATION";
    case VertexAttrib::Tangent:             return "TANGENT_LOCATION";
    case VertexAttrib::Bitangent:           return "BITANGENT_LOCATION";
    case VertexAttrib::Color:               return "COLOR_LOCATION";
    case VertexAttrib::Luminance:           return "LUMINANCE_LOCATION";
    case VertexAttrib::TexCoord:            return "TEXCOORD_LOCATION";
    case VertexAttrib::AO:                  return "AO_LOCATION";
    case VertexAttrib::Size:                return "SIZE_LOCATION";
    case VertexAttrib::Rotation:            return "ROTATION_LOCATION";
    case VertexAttrib::JointID:             return "JOINT_ID_LOCATION";
    case VertexAttrib::JointWeight:         return "JOINT_WEIGHT_LOCATION";
    default:                                return nullptr;
    }
}

const char *GetDescriptorSetMacroName(DescriptorSetType set_type)
{
    switch (set_type)
    {
    case DescriptorSetType::Scene:       return "SCENE_SET";
    case DescriptorSetType::Transform:   return "TRANSFORM_SET";
    case DescriptorSetType::Material:    return "MATERIAL_SET";
    case DescriptorSetType::VertexData:  return "VERTEX_DATA_SET";
    default:                             return nullptr;
    }
}

std::string GetDescriptorBindingMacroName(const char *descriptor_name)
{
    if (!descriptor_name || !*descriptor_name)
        return {};

    // Well-known descriptor names → fixed macro names
    if (std::strcmp(descriptor_name, "viewport") == 0) return "VIEWPORT_BINDING";
    if (std::strcmp(descriptor_name, "camera")   == 0) return "CAMERA_BINDING";
    if (std::strcmp(descriptor_name, "sky")      == 0) return "SKY_BINDING";
    if (std::strcmp(descriptor_name, "l2w")      == 0) return "L2W_BINDING";
    if (std::strcmp(descriptor_name, "tid")      == 0) return "TID_BINDING";
    if (std::strcmp(descriptor_name, "mid")      == 0) return "MID_BINDING";
    if (std::strcmp(descriptor_name, "mtl")      == 0) return "MI_BINDING";

    // Generic: UPPER(name) + "_BINDING"
    std::string result;
    result.reserve(std::strlen(descriptor_name) + 8);
    for (const char *p = descriptor_name; *p; ++p)
        result += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    result += "_BINDING";
    return result;
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
            const char *macro = GetVertexAttribLocationMacroName(via.attrib);
            if (macro)
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

            std::string bind_macro = GetDescriptorBindingMacroName(sd->name);
            if (!bind_macro.empty())
                contract.descriptor_bindings.push_back({ std::move(bind_macro), sd->binding });
        }
    }

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
