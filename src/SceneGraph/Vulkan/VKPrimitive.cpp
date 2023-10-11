﻿#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

#ifdef _DEBUG
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#endif//_DEBUG

VK_NAMESPACE_BEGIN
//bool Renderable::Set(const int stage_input_binding,VIL *vil,VkDeviceSize offset)
//{
//    if(stage_input_binding<0||stage_input_binding>=buf_count||!vil)return(false);
//
//    const VkVertexInputBindingDescription *desc=vertex_sm->GetDesc(stage_input_binding);
//    const VkVertexInputAttributeDescription *attr=vertex_sm->GetAttr(stage_input_binding);
//
//    if(vil->GetFormat()!=attr->format)return(false);
//    if(vil->GetStride()!=desc->stride)return(false);
//
//    //format信息来自于shader，实际中可以不一样。但那样需要为每一个格式产生一个同样shader的material instance，不同的格式又需要不同的pipeline，我们不支持这种行为
//
//    buf_list[stage_input_binding]=vil->GetBuffer();
//    buf_offset[stage_input_binding]=offset;
//
//    return(true);
//}

bool Primitive::Set(const AnsiString &name,VBO *vbo,VkDeviceSize offset)
{
    if(!vbo)return(false);
    if(buffer_list.KeyExist(name))return(false);

    VBOData bd;
    
    bd.buf=vbo;
    bd.offset=offset;

    buffer_list.Add(name,bd);

#ifdef _DEBUG
    auto *da=device->GetDeviceAttribute();

    if(da->debug_maker)
        da->debug_maker->SetBuffer(vbo->GetBuffer(),"[debug maker] "+prim_name+":VBO:"+name);
    if(da->debug_utils)
        da->debug_utils->SetBuffer(vbo->GetBuffer(),"[debug utils] "+prim_name+":VBO:"+name);
#endif//_DEBUG

    return(true);
}

VBO *Primitive::GetVBO(const AnsiString &name,VkDeviceSize *offset)
{
    if(!offset)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    VBOData bd;

    if(buffer_list.Get(name,bd))
    {
        *offset=bd.offset;
        return bd.buf;
    }

    return(nullptr);
}

VkBuffer Primitive::GetBuffer(const AnsiString &name,VkDeviceSize *offset)
{
    VBO *vbo=GetVBO(name,offset);

    if(vbo)return vbo->GetBuffer();

    return(VK_NULL_HANDLE);
}

bool Primitive::Set(IndexBuffer *ib,VkDeviceSize offset)
{
    if(!ib)return(false);

    index_buffer_data.buffer=ib;
    index_buffer_data.offset=offset;

#ifdef _DEBUG
    auto *da=device->GetDeviceAttribute();

    if(da->debug_maker)
        da->debug_maker->SetBuffer(ib->GetBuffer(),"[debug maker] "+prim_name+":IBO");
    if(da->debug_utils)
        da->debug_utils->SetBuffer(ib->GetBuffer(),"[debug utils] "+prim_name+":IBO");
#endif//_DEBUG
    return(true);
}
VK_NAMESPACE_END
