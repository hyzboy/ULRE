#include <hgl/shadergen/SimpleSamplerGLSLEmitter.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/shadergen/ShaderDescriptorInfo.h>
#include <hgl/mtl/SamplerName.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace hgl::graph
{
namespace
{
    static bool CStrEq(const char *lhs, const char *rhs)
    {
        return lhs && rhs && std::strcmp(lhs, rhs) == 0;
    }

    static bool IsSimpleSampler2D(const char *type_name)
    {
        return CStrEq(type_name, "sampler2D");
    }

    static bool IsArraySampler2D(const char *type_name)
    {
        return CStrEq(type_name, "sampler2DArray");
    }

    static void AppendKnownSlotSampler(std::string &out, const ShaderDescriptor *descriptor, const mtl::SamplerSlot slot)
    {
        const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(slot);
        const char *legacy_name = mtl::ToDescriptorName(slot);
        const char *getter_name = mtl::ToGLSLGetterName(slot);

        out += "layout(set=";
        out += std::to_string(descriptor->set);
        out += ", binding=";
        out += std::to_string(descriptor->binding);
        out += ") uniform sampler2D ";
        out += sampler_symbol;
        out += ";\n";

        out += "#ifndef ";
        out += legacy_name;
        out += "\n#define ";
        out += legacy_name;
        out += ' ';
        out += sampler_symbol;
        out += "\n#endif\n";

        out += "vec4 ";
        out += getter_name;
        out += "(vec2 uv)\n{\n    return texture(";
        out += sampler_symbol;
        out += ", uv);\n}\n\n";
    }

    // Generates sampler2DArray binding + layer index global + getter — no #ifdef guards.
    // The emitter preamble is injected before AppendGLSLDefines, so ifdefs cannot be used here.
    static void AppendKnownSlotArraySampler(std::string &out, const ShaderDescriptor *descriptor, const mtl::SamplerSlot slot)
    {
        const char *sampler_symbol = mtl::ToGLSLSamplerSymbol(slot);
        const char *slot_name      = mtl::SamplerSlotNameList[uint8(slot)];
        const char *getter_name    = mtl::ToGLSLGetterName(slot);

        out += "layout(set=";
        out += std::to_string(descriptor->set);
        out += ", binding=";
        out += std::to_string(descriptor->binding);
        out += ") uniform sampler2DArray ";
        out += sampler_symbol;
        out += ";\n";

        // Per-slot layer index — written by _ULRE_InitTextureLayerIndices() at frame start.
        out += "uint _tex_layer_";
        out += slot_name;
        out += ";\n";

        out += "vec4 ";
        out += getter_name;
        out += "(vec2 uv)\n{\n    return texture(";
        out += sampler_symbol;
        out += ", vec3(uv, float(_tex_layer_";
        out += slot_name;
        out += ")));\n}\n\n";
    }

    static void AppendGenericSampler(std::string &out, const ShaderDescriptor *descriptor)
    {
        out += "layout(set=";
        out += std::to_string(descriptor->set);
        out += ", binding=";
        out += std::to_string(descriptor->binding);
        out += ") uniform sampler2D ";
        out += descriptor->name;
        out += ";\n\n";
    }
}

std::string EmitSimpleSamplerGLSL(const ShaderCreateInfo &shader)
{
    const ShaderDescriptorInfo *info = shader.GetShaderDescriptorInfo();
    if (!info)
        return {};

    std::vector<const ShaderDescriptor *> samplers;
    std::vector<const ShaderDescriptor *> array_samplers;

    auto collect = [&](const auto *sd)
    {
        if (!sd || sd->set < 0 || sd->binding < 0) return;
        if (IsSimpleSampler2D(sd->type.c_str()))
            samplers.push_back(sd);
        else if (IsArraySampler2D(sd->type.c_str()))
            array_samplers.push_back(sd);
    };

    for (const TextureDescriptor *texture : info->GetTextureList())
        collect(texture);

    for (const TextureSamplerDescriptor *sampler : info->GetTextureSamplerList())
        collect(sampler);

    if (samplers.empty() && array_samplers.empty())
        return {};

    auto sort_by_binding = [](const ShaderDescriptor *lhs, const ShaderDescriptor *rhs)
    {
        if (lhs->set != rhs->set)
            return lhs->set < rhs->set;
        if (lhs->binding != rhs->binding)
            return lhs->binding < rhs->binding;
        return std::strcmp(lhs->name, rhs->name) < 0;
    };
    std::sort(samplers.begin(),       samplers.end(),       sort_by_binding);
    std::sort(array_samplers.begin(), array_samplers.end(), sort_by_binding);

    std::string out;
    out.reserve(512);
    out += "// ---- Auto-generated simple sampler declarations ----\n";

    for (const ShaderDescriptor *descriptor : samplers)
    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if (mtl::TryGetSlotFromDescriptorName(descriptor->name, slot))
            AppendKnownSlotSampler(out, descriptor, slot);
        else
            AppendGenericSampler(out, descriptor);
    }

    for (const ShaderDescriptor *descriptor : array_samplers)
    {
        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if (mtl::TryGetSlotFromDescriptorName(descriptor->name, slot))
            AppendKnownSlotArraySampler(out, descriptor, slot);
        else
            AppendGenericSampler(out, descriptor);
    }

    out += "// ----------------------------------------------------\n\n";
    return out;
}

std::string EmitMaterialInstanceTextureGLSL(const ShaderCreateInfo &shader)
{
    const ShaderDescriptorInfo *info = shader.GetShaderDescriptorInfo();
    if (!info)
        return {};

    // Collect array sampler slots (same detection as EmitSimpleSamplerGLSL).
    std::vector<mtl::SamplerSlot> array_slots;

    auto try_collect = [&](const auto *sd)
    {
        if (!sd || sd->set < 0 || sd->binding < 0) return;
        if (!IsArraySampler2D(sd->type.c_str())) return;

        mtl::SamplerSlot slot = mtl::SamplerSlot::BaseColor;
        if (mtl::TryGetSlotFromDescriptorName(sd->name, slot))
            array_slots.push_back(slot);
    };

    for (const TextureDescriptor *texture : info->GetTextureList())
        try_collect(texture);
    for (const TextureSamplerDescriptor *sampler : info->GetTextureSamplerList())
        try_collect(sampler);

    if (array_slots.empty())
        return {};

    // Sort by slot ordinal to keep struct field order deterministic.
    std::sort(array_slots.begin(), array_slots.end());

    std::string out;
    out.reserve(512);
    out += "// ---- Auto-generated MaterialInstanceTexture SSBO ----\n";

    // 1. struct MaterialInstanceTexture { uint SlotName; ... };
    out += "struct MaterialInstanceTexture\n{\n";
    for (const mtl::SamplerSlot slot : array_slots)
    {
        out += "    uint ";
        out += mtl::SamplerSlotNameList[uint8(slot)];
        out += ";\n";
    }
    out += "};\n\n";

    // 2. SSBO layout (uses PERMATERIAL_SET / MIT_BINDING from layout defines).
    out += "layout(std430, set=PERMATERIAL_SET, binding=MIT_BINDING) readonly buffer MaterialInstanceTextureID\n";
    out += "{\n    MaterialInstanceTexture tex_id[];\n} mit;\n\n";

    // 3. GetMaterialInstanceTexture(uint instance_id)
    out += "MaterialInstanceTexture GetMaterialInstanceTexture(uint instance_id)\n";
    out += "{\n    return mit.tex_id[instance_id];\n}\n\n";

    // 4. _ULRE_InitTextureLayerIndices(uint instance_id)
    out += "void _ULRE_InitTextureLayerIndices(uint instance_id)\n{\n";
    out += "    MaterialInstanceTexture _m = GetMaterialInstanceTexture(instance_id);\n";
    for (const mtl::SamplerSlot slot : array_slots)
    {
        const char *name = mtl::SamplerSlotNameList[uint8(slot)];
        out += "    _tex_layer_";
        out += name;
        out += " = _m.";
        out += name;
        out += ";\n";
    }
    out += "}\n";

    out += "// ------------------------------------------------------\n\n";
    return out;
}
}
