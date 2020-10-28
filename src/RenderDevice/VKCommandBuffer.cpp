#include<hgl/graph/VKCommandBuffer.h>

VK_NAMESPACE_BEGIN
GPUCmdBuffer::GPUCmdBuffer(VkDevice dev,VkCommandPool cp,VkCommandBuffer cb)
{
    device=dev;
    pool=cp;
    cmd_buf=cb;
}

GPUCmdBuffer::~GPUCmdBuffer()
{
    vkFreeCommandBuffers(device,pool,1,&cmd_buf);
}

bool GPUCmdBuffer::Begin()
{
    CommandBufferBeginInfo cmd_buf_info;
    
    cmd_buf_info.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cmd_buf, &cmd_buf_info)!=VK_SUCCESS)
        return(false);

    return(true);
}
VK_NAMESPACE_END
