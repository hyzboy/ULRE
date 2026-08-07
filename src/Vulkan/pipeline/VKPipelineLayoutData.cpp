#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph{
namespace
{
    static VkDescriptorBindingFlags GetUpdateAfterBindFlags(const VkDescriptorType desc_type)
    {
        switch(desc_type)
        {
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_SAMPLER:
                return VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

            default:
                return 0;
        }
    }

    static VkDescriptorSetLayout CreateEmptyDescriptorSetLayout(VkDevice device)
    {
        VkDescriptorSetLayoutCreateInfo empty_ci{};
        empty_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        empty_ci.bindingCount = 0;

        VkDescriptorSetLayout empty_layout = VK_NULL_HANDLE;
        if(vkCreateDescriptorSetLayout(device, &empty_ci, nullptr, &empty_layout) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return empty_layout;
    }

    static VkDescriptorSetLayout CreateFallbackBindlessSetLayout(VkDevice device)
    {
        VkDescriptorSetLayoutBinding bindings[2]{};

        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = BindlessTextureManager::kMax2D;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[1].descriptorCount = BindlessTextureManager::kMax2DArray;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorBindingFlags binding_flags[2] = {
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
        flags_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flags_ci.bindingCount  = 2;
        flags_ci.pBindingFlags = binding_flags;

        VkDescriptorSetLayoutCreateInfo layout_ci{};
        layout_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_ci.pNext        = &flags_ci;
        layout_ci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_ci.bindingCount = 2;
        layout_ci.pBindings    = bindings;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if(vkCreateDescriptorSetLayout(device, &layout_ci, nullptr, &layout) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return layout;
    }
}

PipelineLayoutData *VulkanDevice::CreatePipelineLayoutData(const MaterialDescriptorManager *desc_manager,
                                                           VkDescriptorSetLayout bindless_layout)
{
    PipelineLayoutData *pld = new PipelineLayoutData();
    memset(pld, 0, sizeof(PipelineLayoutData));
    pld->device = attr->device;

    for(int i = int(DescriptorSetType::Scene); i < int(DescriptorSetType::Bindless); ++i)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;

        if(desc_manager)
        {
            const DescriptorSetLayoutCreateInfo *dslci = desc_manager->GetDSLCI((DescriptorSetType)i);
            if(dslci && dslci->bindingCount > 0)
            {
                ValueArray<VkDescriptorBindingFlags> binding_flags;
                binding_flags.Resize(dslci->bindingCount);

                bool has_update_after_bind = false;
                for(uint32_t binding_index = 0; binding_index < dslci->bindingCount; ++binding_index)
                {
                    const VkDescriptorBindingFlags flags = GetUpdateAfterBindFlags(dslci->pBindings[binding_index].descriptorType);
                    binding_flags[binding_index] = flags;
                    if(flags & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT)
                        has_update_after_bind = true;
                }

                VkDescriptorSetLayoutBindingFlagsCreateInfo flags_ci{};
                flags_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                flags_ci.bindingCount = dslci->bindingCount;
                flags_ci.pBindingFlags = binding_flags.GetData();

                VkDescriptorSetLayoutCreateInfo layout_ci = *dslci;
                if(has_update_after_bind)
                {
                    layout_ci.pNext = &flags_ci;
                    layout_ci.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                }

                if(vkCreateDescriptorSetLayout(attr->device, &layout_ci, nullptr, &layout) != VK_SUCCESS)
                {
                    delete pld;
                    return nullptr;
                }
                pld->vab_count[i] = dslci->bindingCount;
            }
            else
            {
                layout = CreateEmptyDescriptorSetLayout(attr->device);
                if(layout == VK_NULL_HANDLE)
                {
                    delete pld;
                    return nullptr;
                }
                pld->vab_count[i] = 0;
            }
        }
        else
        {
            layout = CreateEmptyDescriptorSetLayout(attr->device);
            if(layout == VK_NULL_HANDLE)
            {
                delete pld;
                return nullptr;
            }
            pld->vab_count[i] = 0;
        }

        pld->layouts[i] = layout;
        pld->fin_dsl[i] = layout;
    }

    constexpr int kBindlessIdx = int(DescriptorSetType::Bindless);
    if(bindless_layout != VK_NULL_HANDLE)
    {
        pld->fin_dsl[kBindlessIdx] = bindless_layout;
        pld->layouts[kBindlessIdx] = VK_NULL_HANDLE;
    }
    else
    {
        VkDescriptorSetLayout fallback_bindless = CreateFallbackBindlessSetLayout(attr->device);
        if(fallback_bindless == VK_NULL_HANDLE)
        {
            delete pld;
            return nullptr;
        }
        pld->layouts[kBindlessIdx] = fallback_bindless;
        pld->fin_dsl[kBindlessIdx] = fallback_bindless;
    }

    pld->bindless_set_index = kBindlessIdx;
    pld->vab_count[kBindlessIdx] = 0;
    pld->fin_dsl_count = uint32_t(DESCRIPTOR_SET_TYPE_COUNT);

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;
    pPipelineLayoutCreateInfo.setLayoutCount            = pld->fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = pld->fin_dsl;
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 0;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = nullptr;

    if(vkCreatePipelineLayout(attr->device,&pPipelineLayoutCreateInfo,nullptr,&(pld->pipeline_layout))!=VK_SUCCESS)
    {
        delete pld;
        return(nullptr);
    }

    return(pld);
}

PipelineLayoutData::~PipelineLayoutData()
{
    if(device == VK_NULL_HANDLE)
        return;

    if(pipeline_layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device,pipeline_layout,nullptr);

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        if(layouts[i])
            vkDestroyDescriptorSetLayout(device,layouts[i],nullptr);

    for(auto pl : placeholder_layouts)
        if(pl != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, pl, nullptr);
}
}//namespace hgl::graph
