#include<hgl/graph/mesh/Primitive.h>
#include<cstring>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/log/Log.h>

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

Primitive::Primitive(Geometry *r,SemanticMaterialId sid,uint32_t vil_hash)
{
    geometry=r;

    data_buffer=nullptr;
    draw_range.Set(geometry);

    deferred_semantic_id=sid;
    deferred_vil_hash=vil_hash;

    // Phase 2c: zero-init MIT for deferred primitives
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

bool Primitive::BindMaterialSlot(const PrimitiveMaterialSlot &slot)
{
    // Note: slot.IsValid() requires mi_id>=0, but we accept mi_id=-1 for non-instanced material binding.
    // Check material_template instead to allow deferred MI slots that resolved material but couldn't allocate MI slot.
    if (!slot.material_template || !slot.vil || !geometry)
        return false;

    // For deferred primitives, create the GeometryDataBuffer from the resolved VIL
    if (HasDeferredMI())
    {
        const uint32_t input_count = slot.vil->GetVertexAttribCount();
        const VertexInputFormat *vif = slot.vil->GetVIFList();

        uint32_t max_binding=0;
        for(uint i=0;i<input_count;i++)
        {
            if(vif[i].binding>max_binding)
                max_binding=vif[i].binding;
        }

        GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(max_binding+1,geometry->GetIBO(),geometry->GetVDM());

        for(uint i=0;i<input_count;i++)
        {
            VAB *vab=geometry->GetVAB(vif->attrib);

            if(!vab)
            {
                delete geom_data_buffer;
                return false;
            }

            const uint32_t bind_index=vif->binding;
            geom_data_buffer->vab_list[bind_index]=vab->GetVkBuffer();
            geom_data_buffer->vab_offset[bind_index]=0;
            GLogDebug("[BIND_SLOT_DEFERRED] prim='%s' bind_idx=%u VkBuffer=%p",
                      geometry->GetName().c_str(), bind_index,
                      (void*)geom_data_buffer->vab_list[bind_index]);
            ++vif;
        }

        delete data_buffer;
        data_buffer=geom_data_buffer;
    }

    // Update all direct fields
    material_template    = slot.material_template;
    domain               = slot.domain;
    mi_id                = slot.mi_id;
    vil                  = slot.vil;
    render_preset        = slot.preset;
    material_preset      = slot.material_preset;
    InitMITLayout(slot.texture_array_slot_flags);
    if (slot.mit_data && mit_packed && slot.mit_data_count > 0)
    {
        const uint32_t copy_count = (slot.mit_data_count < mit_packed_count) ? slot.mit_data_count : mit_packed_count;
        std::memcpy(mit_packed, slot.mit_data, copy_count * sizeof(uint32_t));
    }
    deferred_semantic_id = 0;
    deferred_vil_hash    = 0;

    if (data_buffer)
    {
        static uint32_t s_bind_slot_tick = 0;
        if (++s_bind_slot_tick <= 4u)
        {
            for (uint32_t _i = 0; _i < data_buffer->vab_count; ++_i)
                GLogDebug("[BIND_SLOT_DONE] tick=%u prim='%s' vab[%u]=VkBuffer:%p",
                          s_bind_slot_tick, geometry->GetName().c_str(), _i,
                          (void*)data_buffer->vab_list[_i]);
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Slot-based ctor

Primitive::Primitive(Geometry *r, const PrimitiveMaterialSlot &slot, GeometryDataBuffer *gdb)
{
    geometry   = r;
    data_buffer = gdb;
    draw_range.Set(geometry);

    material_template = slot.material_template;
    domain            = slot.domain;
    mi_id             = slot.mi_id;
    vil               = slot.vil;
    render_preset     = slot.preset;
    material_preset   = slot.material_preset;
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));

    InitMITLayout(slot.texture_array_slot_flags);
    if (slot.mit_data && mit_packed && slot.mit_data_count > 0)
    {
        const uint32_t copy_count = (slot.mit_data_count < mit_packed_count) ? slot.mit_data_count : mit_packed_count;
        std::memcpy(mit_packed, slot.mit_data, copy_count * sizeof(uint32_t));
    }
}

Primitive *DirectCreatePrimitive(Geometry *geom, const PrimitiveMaterialSlot &slot)
{
    if(!geom || !slot.material_template || !slot.vil)
        return nullptr;

    const VIL *vil = slot.vil;
    const uint32_t input_count = vil->GetVertexAttribCount();
    const std::string &mtl_name = slot.material_template->GetName();

    if(geom->GetVABCount() < input_count)
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than MaterialTemplate, MaterialTemplate name: " + mtl_name);
        return nullptr;
    }

    const VertexInputFormat *vif = vil->GetVIFList();

    uint32_t max_binding = 0;
    for(uint i = 0; i < input_count; i++)
    {
        if(vif[i].binding > max_binding)
            max_binding = vif[i].binding;
    }

    GeometryDataBuffer *geom_data_buffer = new GeometryDataBuffer(max_binding + 1, geom->GetIBO(), geom->GetVDM());

    for(uint i = 0; i < input_count; i++)
    {
        VAB *vab = geom->GetVAB(vif->attrib);
        const char *vab_name = GetVertexAttribName(vif->attrib);

        if(!vab)
        {
            GLogError(std::string("[FATAL ERROR] not found VAB \"") + (vab_name ? vab_name : "") +
                      "\" in MaterialTemplate: " + mtl_name);
            delete geom_data_buffer;
            return nullptr;
        }

        if(vab->GetFormat() != vif->format)
        {
            GLogError(std::string("[FATAL ERROR] VAB \"") + (vab_name ? vab_name : "") +
                      "\" format can't match Primitive, MaterialTemplate(" + mtl_name +
                      ") Format(" + GetVulkanFormatName(vif->format) +
                      ") , VAB Format(" + GetVulkanFormatName(vab->GetFormat()) + ")");
            delete geom_data_buffer;
            return nullptr;
        }

        if(vab->GetStride() != vif->stride)
        {
            GLogError(std::string("[FATAL ERROR] VAB \"") + (vab_name ? vab_name : "") +
                      "\" stride can't match Primitive, MaterialTemplate(" + mtl_name +
                      ") stride(" + std::to_string(vif->stride) +
                      ") , VAB stride(" + std::to_string(vab->GetStride()) + ")");
            delete geom_data_buffer;
            return nullptr;
        }

        const uint32_t bind_index = vif->binding;
        geom_data_buffer->vab_list[bind_index]   = vab->GetVkBuffer();
        geom_data_buffer->vab_offset[bind_index] = 0;
        ++vif;
    }

    return new Primitive(geom, slot, geom_data_buffer);
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
