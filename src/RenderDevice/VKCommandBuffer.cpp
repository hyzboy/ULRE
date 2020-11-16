#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDeviceAttribute.h>

VK_NAMESPACE_BEGIN
GPUCmdBuffer::GPUCmdBuffer(const GPUDeviceAttribute *attr,VkCommandBuffer cb)
{
    dev_attr=attr;
    cmd_buf=cb;
}

GPUCmdBuffer::~GPUCmdBuffer()
{
    vkFreeCommandBuffers(dev_attr->device,dev_attr->cmd_pool,1,&cmd_buf);
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
