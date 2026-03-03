#include <hgl/graph/module/ShaderGenDescriptorLayoutAdapter.h>
#include <hgl/vk/VKDescriptorSetType.h>
#include <cstdio>

namespace hgl::graph
{
    namespace
    {
        static VkDescriptorType ToVkDescriptorType(const mtl::contract::ResourceClass rc)
        {
            switch (rc)
            {
                case mtl::contract::ResourceClass::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                case mtl::contract::ResourceClass::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                case mtl::contract::ResourceClass::SampledImage: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                case mtl::contract::ResourceClass::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
                case mtl::contract::ResourceClass::CombinedImageSampler: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                case mtl::contract::ResourceClass::InputAttachment: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
            }
        }
    }

    bool ValidateContractDescriptorLayoutAgainstLegacy(const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                       const mtl::contract::ShaderGenResult &contract_result,
                                                       std::string &reason)
    {
        if (legacy_descriptors.size() != contract_result.layout.bindings.size())
        {
            reason = "descriptor count mismatch";
            return false;
        }

        for (const auto &legacy_desc : legacy_descriptors)
        {
            const mtl::contract::DescriptorBindingDesc *contract_binding = nullptr;
            for (const auto &candidate : contract_result.layout.bindings)
            {
                if ((int)candidate.set == legacy_desc.set && (int)candidate.binding == legacy_desc.binding)
                {
                    contract_binding = &candidate;
                    break;
                }
            }

            if (!contract_binding)
            {
                reason = "missing contract descriptor set=" + std::to_string(legacy_desc.set) + ", binding=" + std::to_string(legacy_desc.binding);
                return false;
            }

            if (ToVkDescriptorType(contract_binding->resource_class) != legacy_desc.desc_type)
            {
                reason = "descriptor type mismatch set=" + std::to_string(legacy_desc.set) + ", binding=" + std::to_string(legacy_desc.binding);
                return false;
            }

            if (contract_binding->stage_mask != legacy_desc.stage_flag)
            {
                reason = "descriptor stage_mask mismatch set=" + std::to_string(legacy_desc.set) + ", binding=" + std::to_string(legacy_desc.binding);
                return false;
            }

            if (contract_binding->name != legacy_desc.name)
            {
                reason = "descriptor name mismatch set=" + std::to_string(legacy_desc.set) + ", binding=" + std::to_string(legacy_desc.binding);
                return false;
            }
        }

        return true;
    }

    bool BuildShaderDescriptorsFromContractLayout(const mtl::contract::ShaderGenResult &contract_result,
                                                  const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                  std::vector<ShaderDescriptor> &descriptors,
                                                  std::string &reason)
    {
        descriptors.clear();
        descriptors.reserve(contract_result.layout.bindings.size());

        for (const auto &binding : contract_result.layout.bindings)
        {
            const VkDescriptorType vk_desc_type = ToVkDescriptorType(binding.resource_class);
            if (vk_desc_type == VK_DESCRIPTOR_TYPE_MAX_ENUM)
            {
                reason = "unknown descriptor resource_class in contract layout set=" + std::to_string(binding.set) + ", binding=" + std::to_string(binding.binding);
                return false;
            }

            ShaderDescriptor desc;
            std::snprintf(desc.name, sizeof(desc.name), "%s", binding.name.c_str());
            desc.desc_type = vk_desc_type;

            desc.set_type = DescriptorSetType::Unknow;
            for (const auto &legacy_desc : legacy_descriptors)
            {
                if (legacy_desc.set == static_cast<int>(binding.set) &&
                    legacy_desc.binding == static_cast<int>(binding.binding))
                {
                    desc.set_type = legacy_desc.set_type;
                    break;
                }
            }

            if (desc.set_type == DescriptorSetType::Unknow)
            {
                if (binding.set < DESCRIPTOR_SET_TYPE_COUNT)
                    desc.set_type = static_cast<DescriptorSetType>(binding.set);
            }

            desc.set = static_cast<int>(binding.set);
            desc.binding = static_cast<int>(binding.binding);
            desc.stage_flag = binding.stage_mask;

            descriptors.emplace_back(desc);
        }

        return true;
    }
}
