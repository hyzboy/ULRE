#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKDeviceRenderPassManage.h>

VK_NAMESPACE_BEGIN
void GPUDevice::InitRenderPassManage()
{
    render_pass_manage=new DeviceRenderPassManage(attr->device,attr->pipeline_cache);
    
    SwapchainRenderbufferInfo rbi(attr->format,attr->physical_device->GetDepthFormat());

    device_render_pass=render_pass_manage->AcquireRenderPass(&rbi);
}

void GPUDevice::ClearRenderPassManage()
{
    SAFE_CLEAR(device_render_pass);
    SAFE_CLEAR(render_pass_manage);
}

RenderPass *GPUDevice::AcquireRenderPass(const RenderbufferInfo *rbi)
{
    for(const VkFormat &fmt:rbi->GetColorFormatList())
        if(!attr->physical_device->IsColorAttachmentOptimal(fmt))
            return(nullptr);
            
    if(rbi->HasDepthOrStencil())
    if(!attr->physical_device->IsDepthAttachmentOptimal(rbi->GetDepthFormat()))
            return(nullptr);

    return render_pass_manage->AcquireRenderPass(rbi);
}
VK_NAMESPACE_END
