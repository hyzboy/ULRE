#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKDeviceAttribute.h>

namespace hgl::graph{
VulkanCmdBuffer::VulkanCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb)
{
    dev_attr=attr;
    cmd_buf=cb;

    cmd_begin=false;
}

VulkanCmdBuffer::~VulkanCmdBuffer()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(dev_attr->device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cmd_buf);

    vkFreeCommandBuffers(dev_attr->device,dev_attr->cmd_pool,1,&cmd_buf);
}

bool VulkanCmdBuffer::Begin()
{
    CommandBufferBeginInfo cmd_buf_info;

    cmd_buf_info.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cmd_buf, &cmd_buf_info)!=VK_SUCCESS)
        return(false);

    cmd_begin=true;
    return(true);
}

#ifdef _DEBUG
void VulkanCmdBuffer::SetDebugName(const std::string &object_name)
{
    if(dev_attr->debug_utils)
    dev_attr->debug_utils->SetCommandBuffer(cmd_buf,object_name.c_str());
}

void VulkanCmdBuffer::BeginRegion(const std::string &region_name,const Color4f &color)
{
    if(dev_attr->debug_utils)
    dev_attr->debug_utils->CmdBegin(cmd_buf,region_name.c_str(),color);
}

void VulkanCmdBuffer::EndRegion()
{
    if(dev_attr->debug_utils)
        dev_attr->debug_utils->CmdEnd(cmd_buf);
}
#endif
}//namespace hgl::graph
