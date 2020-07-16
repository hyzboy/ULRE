#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/VertexAttribBuffer.h>
#include<spirv_cross/spirv_common.hpp>

VK_NAMESPACE_BEGIN
Buffer::~Buffer()
{
    if(buf.memory)delete buf.memory;
    vkDestroyBuffer(device,buf.buffer,nullptr);
}

VertexAttribData *CreateVABCreater(const uint32_t base_type,const uint32_t vecsize,const uint32_t vertex_count)
{
    if(base_type==spirv_cross::SPIRType::SByte)
    {
        if(vecsize==1)return(new VB1i8(vertex_count));else
        if(vecsize==2)return(new VB2i8(vertex_count));else
        //if(vecsize==3)return(new VB3i8(vertex_count));else
        if(vecsize==4)return(new VB4i8(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::UByte)
    {
        if(vecsize==1)return(new VB1u8(vertex_count));else
        if(vecsize==2)return(new VB2u8(vertex_count));else
        //if(vecsize==3)return(new VB3u8(vertex_count));else
        if(vecsize==4)return(new VB4u8(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::Short)
    {
        if(vecsize==1)return(new VB1i16(vertex_count));else
        if(vecsize==2)return(new VB2i16(vertex_count));else
        //if(vecsize==3)return(new VB3i16(vertex_count));else
        if(vecsize==4)return(new VB4i16(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::UShort)
    {
        if(vecsize==1)return(new VB1u16(vertex_count));else
        if(vecsize==2)return(new VB2u16(vertex_count));else
        //if(vecsize==3)return(new VB3u16(vertex_count));else
        if(vecsize==4)return(new VB4u16(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::Int)
    {
        if(vecsize==1)return(new VB1i32(vertex_count));else
        if(vecsize==2)return(new VB2i32(vertex_count));else
        if(vecsize==3)return(new VB3i32(vertex_count));else
        if(vecsize==4)return(new VB4i32(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::UInt)
    {
        if(vecsize==1)return(new VB1u32(vertex_count));else
        if(vecsize==2)return(new VB2u32(vertex_count));else
        if(vecsize==3)return(new VB3u32(vertex_count));else
        if(vecsize==4)return(new VB4u32(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::Half)
    {
        //if(vecsize==1)return(new VB1hf(vertex_count));else
        //if(vecsize==2)return(new VB2hf(vertex_count));else
        //if(vecsize==3)return(new VB3hf(vertex_count));else
        //if(vecsize==4)return(new VB4hf(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::Float)
    {
        if(vecsize==1)return(new VB1f(vertex_count));else
        if(vecsize==2)return(new VB2f(vertex_count));else
        if(vecsize==3)return(new VB3f(vertex_count));else
        if(vecsize==4)return(new VB4f(vertex_count));
    }
    else
    if(base_type==spirv_cross::SPIRType::Double)
    {
        if(vecsize==1)return(new VB1d(vertex_count));else
        if(vecsize==2)return(new VB2d(vertex_count));else
        if(vecsize==3)return(new VB3d(vertex_count));else
        if(vecsize==4)return(new VB4d(vertex_count));
    }

    return nullptr;
}
VK_NAMESPACE_END
