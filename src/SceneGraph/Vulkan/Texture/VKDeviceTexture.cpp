#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
bool GPUDevice::CheckFormatSupport(const VkFormat format,const uint32_t bits,ImageTiling tiling) const
{
    const VkFormatProperties fp=attr->physical_device->GetFormatProperties(format);
    
    if(tiling==ImageTiling::Optimal)
        return(fp.optimalTilingFeatures&bits);
    else
        return(fp.linearTilingFeatures&bits);
}

void GPUDevice::Clear(TextureCreateInfo *tci)
{
    if(!tci)return;

    if(tci->image)DestroyImage(tci->image);
    if(tci->image_view)delete tci->image_view;
    if(tci->memory)delete tci->memory;

    delete tci;
}

bool GPUDevice::SubmitTexture(const VkCommandBuffer *cmd_bufs,const uint32_t count)
{
    if(!cmd_bufs||count<=0)
        return(false);

    texture_queue->Submit(cmd_bufs,count,nullptr,nullptr);
//    texture_queue->WaitQueue();
    texture_queue->WaitFence();

    return(true);
}
VK_NAMESPACE_END
