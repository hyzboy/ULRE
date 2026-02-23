#include<hgl/vk/VKBufferOwner.h>
#include<hgl/vk/VKStagedBuffer.h>   // complete type for ~StagedBuffer via IGPUBuffer*
#include<hgl/vk/VKReBarBuffer.h>    // complete type for ~ReBarBuffer via IGPUBuffer*

namespace hgl::graph{

VkBufferOwner::~VkBufferOwner()
{
    if(staged_source)
    {
        // StagedBuffer or ReBarBuffer owns device_buffer and device_memory — it cleans them up.
        delete staged_source;
        staged_source = nullptr;
        // buf.memory and buf.buffer are aliases into staged_source — already freed.
    }
    else
    {
        if(buf.memory) delete buf.memory;
        if(buf.buffer) vkDestroyBuffer(device, buf.buffer, nullptr);
    }
}

}//namespace hgl::graph
