#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/vk/VKFormat.h>

namespace hgl::graph{
GeometryDataBuffer::GeometryDataBuffer(const uint32_t c,IndexBuffer *ib,VertexDataManager *_vdm)
{
    vab_count=c;

    vab_list=zero_new<VkBuffer>(vab_count);
    vab_offset=zero_new<VkDeviceSize>(vab_count);
    vab_semantic=zero_new<VertexSemantic>(vab_count);

    ibo=ib;
    vdm=_vdm;
}

GeometryDataBuffer::~GeometryDataBuffer()
{
    delete[] vab_offset;
    delete[] vab_list;
    delete[] vab_semantic;
}

void GeometryDrawRange::Set(const Geometry *geometry)
{
    if(!geometry)
    {
        data_vertex_count = 0;
        data_index_count = 0;
        vertex_count = 0;
        index_count = 0;
        vertex_offset = 0;
        first_index = 0;
        return;
    }

    // data counts come from geometry (buffer capacity)
    data_vertex_count = (uint32_t)geometry->GetVertexCount();
    data_index_count  = geometry->GetIndexCount();

    // initialize draw counts to data counts by default
    vertex_count    = data_vertex_count;
    index_count     = data_index_count;

    vertex_offset   = geometry->GetVertexOffset();
    first_index     = geometry->GetFirstIndex();
}

Primitive::Primitive(Geometry *r,ShaderProgram *material,Pipeline *p,GeometryDataBuffer *gdb)
{
    geometry=r;
    pipeline=p;
    material_program = material;

    data_buffer=gdb;
    draw_range.Set(geometry);
}

bool Primitive::UpdateGeometry()
{
    draw_range.Set(geometry);

    // Clamp draw counts if previously set larger than new data counts
    if(draw_range.vertex_count>draw_range.data_vertex_count)
        draw_range.vertex_count = draw_range.data_vertex_count;

    if(draw_range.index_count>draw_range.data_index_count)
        draw_range.index_count = draw_range.data_index_count;

    return data_buffer->Update(geometry);
}

Primitive *DirectCreatePrimitive(Geometry *geom,ShaderProgram *material,Pipeline *p)
{
    if(!geom||!material)
        return(nullptr);

    // 顶点输入统一为 SSBO：按 Geometry 自带语义列表填充顶点数据 SSBO 槽位
    const GeometryVertexFormat &gvf=geom->GetGeometryVertexFormat();
    const uint32_t attr_count=gvf.GetCount();

    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(attr_count,geom->GetIBO(),geom->GetVDM());

    for(uint32_t i=0;i<attr_count;i++)
    {
        const GeometryVertexAttributeFormat *attr=gvf.Get(i);
        if(!attr||attr->semantic==VertexSemantic::Unknown)
            continue;

        const VkBuffer buf=geom->GetVkBuffer(attr->semantic);
        if(buf==VK_NULL_HANDLE)
            continue;

        if(i>=geom_data_buffer->vab_count)
            break;

        geom_data_buffer->vab_list[i]=buf;
        geom_data_buffer->vab_offset[i]=0;
        geom_data_buffer->vab_semantic[i]=attr->semantic;
    }

    return(new Primitive(geom,material,p,geom_data_buffer));
}

Primitive *CreatePrimitiveRuntime(Geometry *geom, ShaderProgram *material, Pipeline *p)
{
    return DirectCreatePrimitive(geom, material, p);
}

bool GeometryDataBuffer::Update(const Geometry *geom)
{
    if(!geom)
        return(false);

    ibo=geom->GetIBO();
    vdm=geom->GetVDM();

    for(uint i=0;i<vab_count;i++)
    {
        vab_list[i]=VK_NULL_HANDLE;
        vab_offset[i]=0;
        vab_semantic[i]=VertexSemantic::Unknown;
    }

    // 按 Geometry 自带的语义格式列表填充——每个语义一个 SSBO 槽位
    //（顶点输入统一为 SSBO：vab_semantic/vab_list 即顶点数据 SSBO 绑定表）
    const GeometryVertexFormat &gvf=geom->GetGeometryVertexFormat();
    const uint32_t attr_count=gvf.GetCount();

    for(uint32_t i=0;i<attr_count;i++)
    {
        const GeometryVertexAttributeFormat *attr=gvf.Get(i);
        if(!attr||attr->semantic==VertexSemantic::Unknown)
            continue;

        const VkBuffer buf=geom->GetVkBuffer(attr->semantic);
        if(buf==VK_NULL_HANDLE)
            continue;

        if(i>=vab_count)    // 语义数超过槽位数（不应发生，防御）
            break;

        vab_list[i]=buf;
        vab_offset[i]=0;
        vab_semantic[i]=attr->semantic;
    }

    return(true);
}

// Primitive draw control APIs
bool Primitive::SetDrawCounts(uint32_t draw_vertex_count,uint32_t draw_index_count)
{
    // only clamp, do not change offsets
    if(draw_vertex_count>draw_range.data_vertex_count)
        draw_vertex_count = draw_range.data_vertex_count;

    if(draw_index_count>draw_range.data_index_count)
        draw_index_count = draw_range.data_index_count;

    draw_range.vertex_count = draw_vertex_count;
    draw_range.index_count  = draw_index_count;

    return true;
}

bool Primitive::SetDrawRange(int32_t vertex_offset,uint32_t first_index,uint32_t draw_vertex_count,uint32_t draw_index_count)
{
    // set offsets
    draw_range.vertex_offset = vertex_offset;
    draw_range.first_index   = first_index;

    // clamp counts to data counts
    if(draw_vertex_count>draw_range.data_vertex_count)
        draw_vertex_count = draw_range.data_vertex_count;

    if(draw_index_count>draw_range.data_index_count)
        draw_index_count = draw_range.data_index_count;

    draw_range.vertex_count = draw_vertex_count;
    draw_range.index_count  = draw_index_count;

    return true;
}

}//namespace hgl::graph
