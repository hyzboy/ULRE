#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include"VKGeometryData.h"

#ifdef _DEBUG
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#endif//_DEBUG

VK_NAMESPACE_BEGIN

//bool Mesh::Set(const int stage_input_binding,VIL *vil,VkDeviceSize offset)
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

Geometry::Geometry(const AnsiString &pn,GeometryData *pd)
{
    geometry_name=pn;
    geometry_data=pd;

    LogVerbose(" Geometry: "+geometry_name);
}

Geometry::~Geometry()
{
    LogVerbose("~Geometry: "+geometry_name);

    SAFE_CLEAR(geometry_data);
}

const VkDeviceSize Geometry::GetVertexCount()const
{
    return geometry_data->GetVertexCount();
}

const uint32_t Geometry::GetVABCount()const
{
    return geometry_data->GetVABCount();
}

const int Geometry::GetVABIndex(const AnsiString &name)const
{
    return geometry_data->GetVABIndex(name);
}

VAB *Geometry::GetVAB(const int vab_index)const
{
    return geometry_data->GetVAB(vab_index);
}

VkBuffer Geometry::GetVkBuffer(const int index)const
{
    VAB *vab=GetVAB(index);
    if(!vab)return(VK_NULL_HANDLE);
    return vab->GetBuffer();
}

VkBuffer Geometry::GetVkBuffer(const AnsiString &name)const
{
    VAB *vab=GetVAB(name);
    if(!vab)return(VK_NULL_HANDLE);
    return vab->GetBuffer();
}

const int32_t Geometry::GetVertexOffset()const
{
    return geometry_data->GetVertexOffset();
}

VABMap *Geometry::GetVABMap(const int vab_index)
{
    return geometry_data->GetVABMap(vab_index);
}

const uint32_t Geometry::GetIndexCount()const
{
    return geometry_data->GetIndexCount();
}

IndexBuffer *Geometry::GetIBO()const
{
    return geometry_data->GetIBO();
}

const uint32_t Geometry::GetFirstIndex()const
{
    return geometry_data->GetFirstIndex();
}

IBMap *Geometry::GetIBMap()
{
    return geometry_data->GetIBMap();
}

VertexDataManager *Geometry::GetVDM()const
{
    return geometry_data->GetVDM();
}
VK_NAMESPACE_END
