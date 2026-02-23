#include<hgl/vk/VKBufferAccessBase.h>
#include<hgl/vk/VKBuffer.h>   // complete type needed for DeviceBuffer* overload

namespace hgl::graph{

void BufferAccessBase::SetBuffer(VkBufferOwner *buf)
{
    buffer  = buf;
    gpu_buf = buf ? buf->GetGPUBuffer() : nullptr;
}

// Backward-compat: stale OBJ files reference SetBuffer(DeviceBuffer*)
void BufferAccessBase::SetBuffer(DeviceBuffer *buf)
{
    SetBuffer(static_cast<VkBufferOwner *>(buf));
}

}//namespace hgl::graph
