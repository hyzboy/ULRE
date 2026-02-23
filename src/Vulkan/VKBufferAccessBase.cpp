#include<hgl/vk/VKBufferAccessBase.h>

namespace hgl::graph{

void BufferAccessBase::SetBuffer(VkBufferOwner *buf)
{
    buffer  = buf;
    gpu_buf = buf ? buf->GetGPUBuffer() : nullptr;
}

}//namespace hgl::graph
