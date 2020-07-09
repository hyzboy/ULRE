#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/VertexBuffer.h>
#include<spirv_cross/spirv_common.hpp>

VK_NAMESPACE_BEGIN
Buffer::~Buffer()
{
    if(buf.memory)delete buf.memory;
    vkDestroyBuffer(device,buf.buffer,nullptr);
}



VertexBufferCreater *CreateVBO(const uint32_t base_type,const uint32_t vecsize,const uint32_t vertex_count)
{
    if(vecsize==1)return(new VB1f(vertex_count));else
    if(vecsize==2)return(new VB2f(vertex_count));else
    if(vecsize==3)return(new VB3f(vertex_count));else
    if(vecsize==4)return(new VB4f(vertex_count));else
}
VK_NAMESPACE_END
