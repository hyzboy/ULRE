#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKDevice.h>
VK_NAMESPACE_BEGIN
RenderPass::~RenderPass()
{
    vkDestroyRenderPass(device,render_pass,nullptr);
}

int RenderpassCreater::AddSwapchainImage()
{
    VkAttachmentDescription desc;

    desc.format         = device->GetSurfaceFormat();
    desc.samples        = VK_SAMPLE_COUNT_1_BIT;
    desc.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    desc.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    desc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return atta_desc_list.Add(desc);
}
VK_NAMESPACE_END
