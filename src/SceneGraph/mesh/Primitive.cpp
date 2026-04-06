#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>

namespace hgl::graph{

GeometryDataBuffer::GeometryDataBuffer(const uint32_t c,IndexBuffer *ib,VertexDataManager *_vdm)
{
    vab_count=c;

    vab_list=zero_new<VkBuffer>(vab_count);
    vab_offset=zero_new<VkDeviceSize>(vab_count);

    ibo=ib;
    vdm=_vdm;
}

GeometryDataBuffer::~GeometryDataBuffer()
{
    delete[] vab_offset;
    delete[] vab_list;
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

Primitive::Primitive(Geometry *r,MaterialInstance *mi,GraphicsPipelinePreRaster *p,GeometryDataBuffer *gdb)
{
    geometry=r;
    mat_inst=mi;

    data_buffer=gdb;
    draw_range.Set(geometry);
}

Primitive::Primitive(Geometry *r,SemanticMaterialId sid,uint32_t vil_hash)
{
    geometry=r;
    mat_inst=nullptr;

    data_buffer=nullptr;
    draw_range.Set(geometry);

    deferred_semantic_id=sid;
    deferred_vil_hash=vil_hash;
}

Primitive::~Primitive()
{
    SAFE_CLEAR(data_buffer);
}

bool Primitive::UpdateGeometry()
{
    draw_range.Set(geometry);

    // Clamp draw counts if previously set larger than new data counts
    if(draw_range.vertex_count>draw_range.data_vertex_count)
        draw_range.vertex_count = draw_range.data_vertex_count;

    if(draw_range.index_count>draw_range.data_index_count)
        draw_range.index_count = draw_range.data_index_count;

    if(!data_buffer||!mat_inst)
        return(false);

    return data_buffer->Update(geometry,mat_inst->GetVIL());
}

bool Primitive::BindMaterialInstance(MaterialInstance *mi)
{
    if(!mi||!geometry)
        return(false);

    const VIL *vil=mi->GetVIL();
    if(!vil)
        return(false);

    const uint32_t input_count=vil->GetVertexAttribCount();
    const VertexInputFormat *vif=vil->GetVIFList();

    uint32_t max_binding=0;
    for(uint i=0;i<input_count;i++)
    {
        if(vif[i].binding>max_binding)
            max_binding=vif[i].binding;
    }

    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(max_binding+1,geometry->GetIBO(),geometry->GetVDM());

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        vab=geometry->GetVAB(vif->attrib);

        if(!vab)
        {
            delete geom_data_buffer;
            return(false);
        }

        const uint32_t bind_index=vif->binding;
        geom_data_buffer->vab_list[bind_index]=vab->GetVkBuffer();
        geom_data_buffer->vab_offset[bind_index]=0;
        ++vif;
    }

    delete data_buffer;
    data_buffer=geom_data_buffer;

    mat_inst=mi;
    deferred_semantic_id=0;

    return(true);
}

Primitive *DirectCreatePrimitive(Geometry *geom,MaterialInstance *mi,GraphicsPipelinePreRaster *p)
//用Direct这个前缀是为了区别于MeshManager/WorkObject等路径上的CreateMesh()
{
    if(!geom||!mi)return(nullptr);

    const VIL *vil=mi->GetVIL();

    if(p && *vil!=*p->GetVIL())
        return(nullptr);

    const uint32_t input_count=vil->GetVertexAttribCount();
    const AnsiString &mtl_name=mi->GetMaterial()->GetName();

    if(geom->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than MaterialTemplate, MaterialTemplate name: "+mtl_name);

        return(nullptr);
    }

    const VertexInputFormat *vif=vil->GetVIFList();

    uint32_t max_binding=0;
    for(uint i=0;i<input_count;i++)
    {
        if(vif[i].binding>max_binding)
            max_binding=vif[i].binding;
    }

    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(max_binding+1,geom->GetIBO(),geom->GetVDM());

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Geometry。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        vab=geom->GetVAB(vif->attrib);

        const char *vab_name=GetVertexAttribName(vif->attrib);

        if(!vab)
        {
            GLogError("[FATAL ERROR] not found VAB \""+AnsiString(vab_name)+"\" in MaterialTemplate: "+mtl_name);
            return(nullptr);
        }

        if(vab->GetFormat()!=vif->format)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vab_name)+
                        AnsiString("\" format can't match Primitive, MaterialTemplate(")+mtl_name+
                        AnsiString(") Format(")+GetVulkanFormatName(vif->format)+
                        AnsiString(") , VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=vif->stride)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vab_name)+
                        AnsiString("\" stride can't match Primitive, MaterialTemplate(")+mtl_name+
                        AnsiString(") stride(")+AnsiString::numberOf(vif->stride)+
                        AnsiString(") , VAB stride(")+AnsiString::numberOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        const uint32_t bind_index=vif->binding;
        geom_data_buffer->vab_list[bind_index]=vab->GetVkBuffer();
        geom_data_buffer->vab_offset[bind_index]=0;
        ++vif;
    }

    return(new Primitive(geom,mi,p,geom_data_buffer));
}

Primitive *DirectCreatePrimitive(Geometry *geom,SemanticMaterialId sid,uint32_t vil_hash)
{
    if(!geom||sid==0)
        return(nullptr);

    return(new Primitive(geom,sid,vil_hash));
}

bool GeometryDataBuffer::Update(const Geometry *geom,const VIL *vil)
{
    if(!geom||!vil)
        return(false);

    ibo=geom->GetIBO();
    vdm=geom->GetVDM();

    for(uint i=0;i<vab_count;i++)
    {
        vab_list[i]=VK_NULL_HANDLE;
        vab_offset[i]=0;
    }

    const uint32_t input_count=vil->GetVertexAttribCount();
    const VertexInputFormat *vif=vil->GetVIFList();

    for(uint i=0;i<input_count;i++)
    {
        if(vif->binding<vab_count)
        {
            vab_list[vif->binding]=geom->GetVkBuffer(vif->attrib);
            vab_offset[vif->binding]=0;
        }

        ++vif;
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
