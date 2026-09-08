#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph{
namespace
{
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

PipelineLayoutData *VulkanDevice::CreatePipelineLayoutData(VkDescriptorSetLayout bindless_layout,
                                                           VkDescriptorSetLayout scene_layout)
{
    PipelineLayoutData *pld = new PipelineLayoutData();
    memset(pld, 0, sizeof(PipelineLayoutData));
    pld->device = attr->device;

    // A6-2b/b3 终态：描述符集只剩 Scene(0)/Bindless(1) 两个设备级全局集——
    // per-material 集与 desc_manager/MP 机制已整体退役，此处不再构建任何材质布局。
    for(int i = int(DescriptorSetType::Scene); i < int(DESCRIPTOR_SET_TYPE_COUNT); ++i)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;

        if(i == int(DescriptorSetType::Scene))
        {
            // 全局 Scene UBO 集：layout 由设备级 GlobalSceneUBOSet 提供，所有材质共用同一 layout。
            if(scene_layout != VK_NULL_HANDLE)
            {
                pld->fin_dsl[i] = scene_layout;
                pld->layouts[i] = VK_NULL_HANDLE;
                continue;
            }

            // 全局集未就绪时的占位空 layout 兜底（正常 Init 顺序下不出现）
            layout = CreateEmptyDescriptorSetLayout(attr->device);
            if(layout == VK_NULL_HANDLE)
            {
                delete pld;
                return nullptr;
            }
        }
        else
        {
            // Bindless：layout 由 bindless_layout 提供（循环外统一设），此处仅置空
            pld->layouts[i] = VK_NULL_HANDLE;
            continue;
        }

        pld->layouts[i] = layout;
        pld->fin_dsl[i] = layout;
    }

    constexpr int kBindlessIdx = int(DescriptorSetType::Bindless);
    // Bindless（Set 1）恒为设备级全局 layout（BindlessTextureManager 提供），
    // 由 GraphicsContext::Init 保证非空；不存在回退布局路径。
    pld->fin_dsl[kBindlessIdx] = bindless_layout;
    pld->layouts[kBindlessIdx] = VK_NULL_HANDLE;

    pld->bindless_set_index = kBindlessIdx;
    pld->fin_dsl_count = uint32_t(DESCRIPTOR_SET_TYPE_COUNT);

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;
    pPipelineLayoutCreateInfo.setLayoutCount            = pld->fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = pld->fin_dsl;
    // RootAddresses：push constant 承载 7 张全局表设备地址（56B，HGL_ROOT_ADDRESSES_FIELD_LIST）。
    // stage=Mesh|Fragment（kMeshFragment）——mesh 读 MeshDrawParams/L2W/L2WIndex/文本表，
    // FS 读 mtl_data_addrs。IndirectMeshDraw 的 per-draw 段偏移仍走参数表 rows[gl_DrawID]
    // 查表（push constant 放的是表地址，不是 per-draw 数据——一次 vkCmdDrawMeshTasksIndirectEXT
    // 内 N 命令共享同一份，合法）。
    const VkPushConstantRange root_address_range =
    {
        VkShaderStageFlags(hgl::graph::kMeshFragment),
        0,
        uint32_t(sizeof(hgl::graph::mtl::RootAddresses))
    };
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = &root_address_range;

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
