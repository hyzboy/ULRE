#pragma once

#include<hgl/common/DescriptorSetTypeDef.h>
#include<hgl/mtl/new/SurfaceType.h>
#include<vulkan/vulkan.h>

namespace hgl::graph{

constexpr uint32_t NEW_DS_COUNT = static_cast<uint32_t>(DESCRIPTOR_SET_TYPE_COUNT);

struct NewPipelineLayoutData
{
    VkDevice device = VK_NULL_HANDLE;

    VkDescriptorSetLayout layouts[NEW_DS_COUNT] = {};
    VkPipelineLayout      pipeline_layout       = VK_NULL_HANDLE;

    ~NewPipelineLayoutData();
};

namespace NewDescriptorSetLayoutFactory
{
    VkDescriptorSetLayout CreatePerSceneLayout   (VkDevice device);
    VkDescriptorSetLayout CreatePerViewLayout    (VkDevice device);
    VkDescriptorSetLayout CreatePerMaterialLayout(VkDevice device, SurfaceType surface_type);
    VkDescriptorSetLayout CreatePerDrawLayout    (VkDevice device, bool ssbo_platform);

    NewPipelineLayoutData *CreateNewPipelineLayout(VkDevice device, SurfaceType surface_type, bool ssbo_platform);
}

}//namespace hgl::graph
