#include <hgl/shadergen/contract/ShaderGenRequestBuilder.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/mtl/DescriptorBindingContract.h>
#include <hgl/graph/shared/VertexAttribDef.h>
#include <cstring>

namespace hgl::graph::mtl::contract
{
    static ResourceClass ToResourceClass(const DescriptorKind kind)
    {
        switch (kind)
        {
            case DescriptorKind::UBO: return ResourceClass::UniformBuffer;
            case DescriptorKind::SSBO: return ResourceClass::StorageBuffer;
            case DescriptorKind::Texture: return ResourceClass::SampledImage;
            case DescriptorKind::TextureSampler: return ResourceClass::CombinedImageSampler;
            default: return ResourceClass::Unknown;
        }
    }

    static uint32_t HashMaterialName(const char *name)
    {
        if (!name || !name[0])
            return 0;

        uint32_t hash = 2166136261u;
        const unsigned char *p = reinterpret_cast<const unsigned char *>(name);
        while (*p)
        {
            hash ^= static_cast<uint32_t>(*p++);
            hash *= 16777619u;
        }
        return hash;
    }

    bool BuildShaderGenRequestFromMaterialCreateInfo(const MaterialCreateInfo &mci,
                                                     const PhysicalDeviceProfileLite *physical_device_profile,
                                                     ShaderGenRequest &request,
                                                     const char *material_name)
    {
        request = ShaderGenRequest{};
        request.contract_version = kShaderGenContractVersion;

        request.material_id = HashMaterialName(material_name);
        request.material_cfg.primitive_type = static_cast<uint32_t>(mci.GetPrimitiveType());
        request.material_cfg.shader_stage_flags = mci.GetShaderStage();
        request.material_cfg.enable_lighting = false;

        const BindingContract &contract = mci.GetBindingContract();
        request.required_resources.reserve(contract.requirements.size());

        for (const DescriptorRequirement &req : contract.requirements)
        {
            ResourceRequirement item;
            item.name = req.name ? req.name : GetDescriptorSemanticName(req.semantic);
            item.resource_class = ToResourceClass(req.kind);
            item.required = req.required;
            request.required_resources.emplace_back(std::move(item));
        }

        if (auto *vsc = mci.GetVS())
        {
            const auto &inputs = vsc->GetInput();
            request.vertex_requirements.reserve(inputs.count);

            for (uint32_t i = 0; i < inputs.count; ++i)
            {
                const auto &via = inputs.items[i];

                VertexInputRequirement item;
                item.semantic = via.name;
                item.location = via.location;
                item.type_name = GetVertexAttribName((VABaseType)via.basetype, via.vec_size);
                item.input_rate = via.input_rate;
                request.vertex_requirements.emplace_back(std::move(item));
            }
        }

        if (physical_device_profile)
        {
            request.has_physical_device_profile = true;
            request.physical_device_profile = *physical_device_profile;
        }

        return true;
    }

    bool BuildShaderGenRequestFromMaterialCreateInfo(const MaterialCreateInfo &mci,
                                                     ShaderGenRequest &request,
                                                     const char *material_name)
    {
        return BuildShaderGenRequestFromMaterialCreateInfo(mci,
                                                           nullptr,
                                                           request,
                                                           material_name);
    }
}
