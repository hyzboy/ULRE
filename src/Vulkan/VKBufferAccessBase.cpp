#include<hgl/vk/VKBufferAccessBase.h>

namespace hgl::graph{

void BufferAccessBase::SetBuffer(DeviceBuffer *buf)
{
    buffer = buf;
    dirty  = false;
}

}//namespace hgl::graph
