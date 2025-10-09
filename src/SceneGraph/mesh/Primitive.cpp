#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

VK_NAMESPACE_BEGIN
GeometryDataBuffer::GeometryDataBuffer(const uint32_t c,IndexBuffer *ib,VertexDataManager *_vdm)
{
    vab_count=c;

    vab_list=hgl_zero_new<VkBuffer>(vab_count);
    vab_offset=hgl_zero_new<VkDeviceSize>(vab_count);

    ibo=ib;
    vdm=_vdm;
}

GeometryDataBuffer::~GeometryDataBuffer()
{
    delete[] vab_offset;
    delete[] vab_list;
}

const int GeometryDataBuffer::compare(const GeometryDataBuffer &gdb)const
{
    ptrdiff_t off;

    if(vdm&&gdb.vdm)
    {
        off=(ptrdiff_t)vdm-(ptrdiff_t)gdb.vdm;
        if(off)
            return off;
    }

    off=vab_count-gdb.vab_count;
    if(off)
        return off;

    off=hgl_cmp(vab_list,gdb.vab_list,vab_count);
    if(off)
        return off;

    off=hgl_cmp(vab_offset,gdb.vab_offset,vab_count);
    if(off)
        return off;

    off=ibo-gdb.ibo;

    return off;
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

Primitive::Primitive(Geometry *r,MaterialInstance *mi,Pipeline *p,GeometryDataBuffer *gdb)
{
    geometry=r;
    pipeline=p;
    mat_inst=mi;

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

    return data_buffer->Update(geometry,mat_inst->GetVIL());
}

Primitive *DirectCreatePrimitive(Geometry *geom,MaterialInstance *mi,Pipeline *p)
//用Direct这个前缀是为了区别于MeshManager/WorkObject/RenderFramework的::CreateMesh()
{
    if(!geom||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();

    if(*vil!=*p->GetVIL())
        return(nullptr);

    const uint32_t input_count=vil->GetVertexAttribCount(VertexInputGroup::Basic);       //不统计Bone/LocalToWorld组的
    const AnsiString &mtl_name=mi->GetMaterial()->GetName();

    if(geom->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(input_count,geom->GetIBO(),geom->GetVDM());

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Geometry。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        vab=geom->GetVAB(vif->name);

        if(!vab)
        {
            GLogError("[FATAL ERROR] not found VAB \""+AnsiString(vif->name)+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vab->GetFormat()!=vif->format)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vif->name)+
                        AnsiString("\" format can't match Primitive, Material(")+mtl_name+
                        AnsiString(") Format(")+GetVulkanFormatName(vif->format)+
                        AnsiString(") , VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=vif->stride)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vif->name)+
                        AnsiString("\" stride can't match Primitive, Material(")+mtl_name+
                        AnsiString(") stride(")+AnsiString::numberOf(vif->stride)+
                        AnsiString(") , VAB stride(")+AnsiString::numberOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        geom_data_buffer->vab_list[i]=vab->GetBuffer();
        geom_data_buffer->vab_offset[i]=0;
        ++vif;
    }

    return(new Primitive(geom,mi,p,geom_data_buffer));
}

bool GeometryDataBuffer::Update(const Geometry *geom,const VIL *vil)
{
    if(!geom||!vil)
        return(false);

    ibo=geom->GetIBO();
    vdm=geom->GetVDM();

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);

    for(uint i=0;i<vab_count;i++)
    {
        vab_list[i]=geom->GetVkBuffer(vif->name);
        vab_offset[i]=0;

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

VK_NAMESPACE_END
