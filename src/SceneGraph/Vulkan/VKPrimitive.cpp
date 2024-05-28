#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include"VKPrimitiveData.h"

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

Primitive::Primitive(const AnsiString &pn,PrimitiveData *pd)
{
    prim_name=pn;
    prim_data=pd;
}

Primitive::~Primitive()
{
    SAFE_CLEAR(prim_data);
}

const VkDeviceSize Primitive::GetVertexCount()const
{
    return prim_data->GetVertexCount();
}

const int Primitive::GetVABCount()const
{
    return prim_data->GetVABCount();
}

const int Primitive::GetVABIndex(const AnsiString &name)const
{
    return prim_data->GetVABIndex(name);
}

VAB *Primitive::GetVAB(const int vab_index)
{
    return prim_data->GetVAB(vab_index);
}

const int32_t Primitive::GetVertexOffset()const
{
    return prim_data->GetVertexOffset();
}

const uint32_t Primitive::GetIndexCount()const
{
    return prim_data->GetIndexCount();
}

IndexBuffer *Primitive::GetIBO()
{
    return prim_data->GetIBO();
}

const uint32_t Primitive::GetFirstIndex()const
{
    return prim_data->GetFirstIndex();
}
VK_NAMESPACE_END
