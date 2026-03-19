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

    static void AppendKnownSlotSampler(std::string &out, const ShaderDescriptor *descriptor, const mtl::SamplerName::SamplerSlot slot)
    {
        const char *sampler_symbol = mtl::SamplerName::ToGLSLSamplerSymbol(slot);
        const char *legacy_name = mtl::SamplerName::ToDescriptorName(slot);
        const char *getter_name = mtl::SamplerName::ToGLSLGetterName(slot);

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

    for (const TextureDescriptor *texture : info->GetTextureList())
    {
        if (texture && texture->set >= 0 && texture->binding >= 0 && IsSimpleSampler2D(texture->type.c_str()))
            samplers.push_back(texture);
    }

    for (const TextureSamplerDescriptor *sampler : info->GetTextureSamplerList())
    {
        if (sampler && sampler->set >= 0 && sampler->binding >= 0 && IsSimpleSampler2D(sampler->type.c_str()))
            samplers.push_back(sampler);
    }

    if (samplers.empty())
        return {};

    std::sort(samplers.begin(), samplers.end(), [](const ShaderDescriptor *lhs, const ShaderDescriptor *rhs)
    {
        if (lhs->set != rhs->set)
            return lhs->set < rhs->set;
        if (lhs->binding != rhs->binding)
            return lhs->binding < rhs->binding;
        return std::strcmp(lhs->name, rhs->name) < 0;
    });

    std::string out;
    out.reserve(512);
    out += "// ---- Auto-generated simple sampler declarations ----\n";

    for (const ShaderDescriptor *descriptor : samplers)
    {
        mtl::SamplerName::SamplerSlot slot = mtl::SamplerName::SamplerSlot::BaseColor;
        if (mtl::SamplerName::TryGetSlotFromDescriptorName(descriptor->name, slot))
            AppendKnownSlotSampler(out, descriptor, slot);
        else
            AppendGenericSampler(out, descriptor);
    }

    out += "// ----------------------------------------------------\n\n";
    return out;
}
}