#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/manager/RenderPassManage.h>

VK_NAMESPACE_BEGIN
void GPUDevice::InitRenderPassManage()
{
    render_pass_manage=new RenderPassManager(this);
    
    SwapchainRenderbufferInfo rbi(attr->surface_format.format,attr->physical_device->GetDepthFormat());

    device_render_pass=render_pass_manage->AcquireRenderPass(&rbi);

    #ifdef _DEBUG
        if(attr->debug_utils)
            attr->debug_utils->SetRenderPass(device_render_pass->GetVkRenderPass(),"MainDeviceRenderPass");
    #endif//_DEBUG
}

void GPUDevice::ClearRenderPassManage()
{
    SAFE_CLEAR(render_pass_manage);
}
VK_NAMESPACE_END
