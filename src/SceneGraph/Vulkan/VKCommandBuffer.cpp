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

#ifdef _DEBUG
void GPUCmdBuffer::SetDebugName(const char *object_name)
{
    if(dev_attr->debug_maker)
        dev_attr->debug_maker->SetCommandBuffer(cmd_buf,object_name);
    
    if(dev_attr->debug_utils)
        dev_attr->debug_utils->SetCommandBuffer(cmd_buf,object_name);
}

void GPUCmdBuffer::BeginRegion(const char *region_name,const Color4f &color)
{
    if(dev_attr->debug_maker)
        dev_attr->debug_maker->Begin(cmd_buf,region_name,color);

    if(dev_attr->debug_utils)
        dev_attr->debug_utils->Begin(cmd_buf,region_name,color);
}

void GPUCmdBuffer::EndRegion()
{
    if(dev_attr->debug_maker)
        dev_attr->debug_maker->End(cmd_buf);

    if(dev_attr->debug_utils)
        dev_attr->debug_utils->End(cmd_buf);
}
#endif
VK_NAMESPACE_END
