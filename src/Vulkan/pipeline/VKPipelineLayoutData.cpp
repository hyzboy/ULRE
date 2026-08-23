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
}

PipelineLayoutData *VulkanDevice::CreatePipelineLayoutData(const MaterialDescriptorManager *desc_manager,
                                                           VkDescriptorSetLayout bindless_layout,
                                                           VkDescriptorSetLayout scene_layout)
{
    PipelineLayoutData *pld = new PipelineLayoutData();
    memset(pld, 0, sizeof(PipelineLayoutData));
    pld->device = attr->device;

    for(int i = int(DescriptorSetType::Scene); i < int(DescriptorSetType::Bindless); ++i)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;

        if(i == int(DescriptorSetType::Scene) && scene_layout != VK_NULL_HANDLE)
        {
            // 全局 Scene UBO 集（P1）：layout 由设备级 GlobalSceneUBOSet 提供，
            // 所有材质共用同一 layout；不构建 per-material 布局、不分配 per-material MP。
            pld->fin_dsl[i] = scene_layout;
            pld->layouts[i] = VK_NULL_HANDLE;
            pld->vab_count[i] = 0;
            continue;
        }

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
    // Bindless（Set 3）恒为设备级全局 layout（BindlessTextureManager 提供），
    // 由 GraphicsContext::Init 保证非空；不存在回退布局路径。
    pld->fin_dsl[kBindlessIdx] = bindless_layout;
    pld->layouts[kBindlessIdx] = VK_NULL_HANDLE;

    pld->bindless_set_index = kBindlessIdx;
    pld->vab_count[kBindlessIdx] = 0;
    pld->fin_dsl_count = uint32_t(DESCRIPTOR_SET_TYPE_COUNT);

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;
    pPipelineLayoutCreateInfo.setLayoutCount            = pld->fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = pld->fin_dsl;
    // IndirectMeshDraw：per-draw 段偏移改经 mesh_draw_params 参数表 SSBO 传递
    //（rows[gl_DrawID] 查表），mesh shader 不再使用 push constant——range 已删除
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
