#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>

#ifdef _DEBUG
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKDeviceAttribute.h>
#endif//_DEBUG

namespace hgl::graph{

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

const int Geometry::GetVABIndex(const VertexSemantic semantic)const
{
    return geometry_data->GetVABIndex(semantic);
}

VAB *Geometry::GetVAB(const int vab_index)const
{
    return geometry_data->GetVAB(vab_index);
}

VAB *Geometry::GetVAB(const VertexSemantic semantic)const
{
    return geometry_data->GetVAB(semantic);
}

VkBuffer Geometry::GetVkBuffer(const int index)const
{
    VAB *vab=GetVAB(index);
    if(!vab)return(VK_NULL_HANDLE);
    return vab->GetVkBuffer();
}

VkBuffer Geometry::GetVkBuffer(const VertexSemantic semantic)const
{
    VAB *vab=GetVAB(semantic);
    if(!vab)return(VK_NULL_HANDLE);
    return vab->GetVkBuffer();
}

const int32_t Geometry::GetVertexOffset()const
{
    return geometry_data->GetVertexOffset();
}

const uint32_t Geometry::GetIndexCount()const
{
    return geometry_data->GetIndexCount();
}

const GeometryVertexFormat &Geometry::GetGeometryVertexFormat()const
{
    return geometry_data->GetGeometryVertexFormat();
}

IndexBuffer *Geometry::GetIBO()const
{
    return geometry_data->GetIBO();
}

const uint32_t Geometry::GetFirstIndex()const
{
    return geometry_data->GetFirstIndex();
}

VertexDataManager *Geometry::GetVDM()const
{
    return geometry_data->GetVDM();
}
}//namespace hgl::graph
