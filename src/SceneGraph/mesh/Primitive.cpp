#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<cstring>
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

    // Phase 2a: populate direct fields from MI (Primitive is friend of MaterialInstance)
    if(mi)
    {
        material_template = mi->GetMaterial();
        domain            = mi->GetDomain();
        mi_id             = mi->GetMIID();
        vil               = mi->GetVIL();
        render_preset     = mi->GetRenderPreset();
        std::memcpy(mit_slot_offset, mi->mit_slot_offset, sizeof(mit_slot_offset));
        mit_packed_count  = mi->mit_packed_count;
        if(mit_packed_count > 0)
        {
            mit_packed = new uint32_t[mit_packed_count];
            std::memcpy(mit_packed, mi->mit_packed, mit_packed_count * sizeof(uint32_t));
        }
    }
    else
    {
        std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));
    }
}

Primitive::Primitive(Geometry *r,SemanticMaterialId sid,uint32_t vil_hash)
{
    geometry=r;
    mat_inst=nullptr;

    data_buffer=nullptr;
    draw_range.Set(geometry);

    deferred_semantic_id=sid;
    deferred_vil_hash=vil_hash;

    // Phase 2a: zero-init MIT for deferred primitives
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));
}

Primitive::~Primitive()
{
    SAFE_CLEAR(data_buffer);
    delete[] mit_packed;
}

bool Primitive::UpdateGeometry()
{
    draw_range.Set(geometry);

    // Clamp draw counts if previously set larger than new data counts
    if(draw_range.vertex_count>draw_range.data_vertex_count)
        draw_range.vertex_count = draw_range.data_vertex_count;

    if(draw_range.index_count>draw_range.data_index_count)
        draw_range.index_count = draw_range.data_index_count;

    if(!data_buffer||!vil)
        return(false);

    return data_buffer->Update(geometry,vil);
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
    this->vil=vil;  // local var already validated above
    deferred_semantic_id=0;

    // Phase 2a: sync direct fields from MI
    material_template = mi->GetMaterial();
    domain            = mi->GetDomain();
    mi_id             = mi->GetMIID();
    render_preset     = mi->GetRenderPreset();
    delete[] mit_packed;
    mit_packed        = nullptr;
    mit_packed_count  = 0;
    std::memcpy(mit_slot_offset, mi->mit_slot_offset, sizeof(mit_slot_offset));
    mit_packed_count  = mi->mit_packed_count;
    if(mit_packed_count > 0)
    {
        mit_packed = new uint32_t[mit_packed_count];
        std::memcpy(mit_packed, mi->mit_packed, mit_packed_count * sizeof(uint32_t));
    }

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

// ---------------------------------------------------------------------------
// Phase 2a — MI data methods (mirrors MaterialInstance implementation)
// ---------------------------------------------------------------------------

void Primitive::WriteMIData(const void *data, uint32_t size)
{
    if(!data || !size || !material_template || size > material_template->GetMIDataBytes())
        return;

    void *tp = GetMIData();
    if(tp)
        std::memcpy(tp, data, size);
}

void Primitive::InitMITLayout(uint8_t slot_flags)
{
    delete[] mit_packed;
    mit_packed       = nullptr;
    mit_packed_count = 0;
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));

    if(!slot_flags) return;

    uint32_t offset = 0;
    for(uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
    {
        if(slot_flags & (1u << s))
        {
            mit_slot_offset[s] = static_cast<int8_t>(offset);
            ++offset;
        }
    }
    mit_packed_count = offset;
    mit_packed = new uint32_t[mit_packed_count];
    std::memset(mit_packed, 0, mit_packed_count * sizeof(uint32_t));
}

void Primitive::SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer)
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if(off < 0) return;
    mit_packed[off] = layer;
}

uint32_t Primitive::GetTextureArrayLayer(mtl::SamplerSlot slot) const
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if(off < 0) return 0;
    return mit_packed[off];
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
