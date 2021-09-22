#ifndef HGL_VULKAN_DEVICE_RENDERPASS_MANAGE_INCLUDE
#define HGL_VULKAN_DEVICE_RENDERPASS_MANAGE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN
using RenderPassHASHCode=util::HashCodeSHA1LE;

class DeviceRenderPassManage
{
    VkDevice device;
    VkPipelineCache pipeline_cache;

    util::Hash *hash;

    Map<RenderPassHASHCode,RenderPass *> RenderPassList;

private:

    friend class GPUDevice;

    DeviceRenderPassManage(VkDevice,VkPipelineCache);
    ~DeviceRenderPassManage();

private:
    
    RenderPass *    CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const RenderbufferInfo *);

    RenderPass *    AcquireRenderPass(   const RenderbufferInfo *);
};//class DeviceRenderPassManage
VK_NAMESPACE_END
#endif//HGL_VULKAN_DEVICE_RENDERPASS_MANAGE_INCLUDE
