#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/MaterialDescriptorInfo.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/vk/VKVertexInputAttribute.h>
#include <vector>

namespace hgl::graph::mtl::contract
{
    static ResourceClass ToResourceClass(const VkDescriptorType desc_type)
    {
        switch(desc_type)
        {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return ResourceClass::UniformBuffer;
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return ResourceClass::StorageBuffer;
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return ResourceClass::SampledImage;
            case VK_DESCRIPTOR_TYPE_SAMPLER: return ResourceClass::Sampler;
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return ResourceClass::CombinedImageSampler;
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return ResourceClass::InputAttachment;
            default: return ResourceClass::Unknown;
        }
    }

    bool BuildShaderGenResultFromMaterialCreateInfo(const MaterialCreateInfo &mci, ShaderGenResult &result)
    {
        result = ShaderGenResult{};
        result.contract_version = kShaderGenContractVersion;

        const auto &sds_array = mci.GetMDI().Get();

        for(size_t i=0;i<DESCRIPTOR_SET_TYPE_COUNT;i++)
        {
            std::vector<ShaderDescriptor*> values;
            sds_array[i].descriptor_map.GetValueArray(values);

            for(const auto *sd:values)
            {
                if(!sd)
                    continue;

                DescriptorBindingDesc item;
                item.set = sd->set>=0?uint32_t(sd->set):uint32_t(i);
                item.binding = sd->binding>=0?uint32_t(sd->binding):0u;
                item.resource_class = ToResourceClass(sd->desc_type);
                item.stage_mask = sd->stage_flag;
                item.name = sd->name;
                result.layout.bindings.emplace_back(std::move(item));
            }
        }

        if(auto *vsc = mci.GetVS())
        {
            const auto &inputs = vsc->GetInput();
            for(uint32_t i=0;i<inputs.count;i++)
            {
                const auto &via = inputs.items[i];

                VertexAttributeDesc attr;
                attr.location = via.location;
                attr.semantic = via.name;
                attr.type_name = GetVertexAttribName((VABaseType)via.basetype,via.vec_size);
                attr.input_rate = via.input_rate;
                result.vertex_layout.attributes.emplace_back(std::move(attr));
            }
        }

        const auto &shader_map = mci.GetShaderMap();
        for(const auto &kv : shader_map)
        {
            const ShaderCreateInfo *sc = kv.second;
            if(!sc)
                continue;

            const uint32_t *spv_data = sc->GetSPVData();
            const size_t spv_length = sc->GetSPVSize();
            if(!spv_data || spv_length == 0)
                continue;

            StageSpvBlob blob;
            blob.stage_mask = static_cast<uint32_t>(sc->GetShaderStage());
            blob.words.assign(spv_data, spv_data + (spv_length / sizeof(uint32_t)));
            result.spv_per_stage.emplace_back(std::move(blob));
        }

        if(result.spv_per_stage.empty())
            result.diagnostics.warnings.emplace_back("ShaderGenResult mirror builder: no SPV blobs exported from shader_map.");

        return true;
    }
}
