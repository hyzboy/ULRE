#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKBufferAccessBase.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph{
DeviceBuffer::~DeviceBuffer()
{
    if (owner_device)
        owner_device->UntrackBuffer(this);

    if(auto_commit_proxy)
    {
        delete auto_commit_proxy;
        auto_commit_proxy=nullptr;
    }

    if(transfer_agent)
    {
        delete transfer_agent;
        transfer_agent=nullptr;
        buf.memory=nullptr;
        buf.buffer=nullptr;
        return;
    }

    if(buf.memory)delete buf.memory;
    if(buf.buffer)vkDestroyBuffer(device,buf.buffer,nullptr);
}
}//namespace hgl::graph
