#include<hgl/graph/mesh/Primitive.h>
#include<cstring>
#include<cstdint>
#include<hgl/graph/module/VertexBindingCompatibility.h>
#include<hgl/graph/module/VertexBindingDiagnostics.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

namespace
{
uint32_t GetMaxBindingIndex(const VIL *vil)
{
    if(!vil)
        return 0;

    uint32_t max_binding = 0;
    const uint32_t input_count = vil->GetVertexAttribCount();
    const VertexInputFormat *vif = vil->GetVIFList();

    for(uint32_t i = 0; i < input_count; ++i)
    {
        if(vif[i].binding > max_binding)
            max_binding = vif[i].binding;
    }

    return max_binding;
}

bool ResolveEffectivePrimitiveVIL(const Geometry *geom,
                                  MaterialTemplate *material,
                                  const VIL *requested_vil,
                                  const char *compat_log_tag,
                                  const std::string &material_name,
                                  const VIL *&effective_vil,
                                  VIL *&owned_effective_vil,
                                  std::string &reason)
{
    if(!geom || !material || !requested_vil)
    {
        reason = "geom_material_or_vil_missing";
        return false;
    }

    VILConfig runtime_cfg;
    bool has_any = false;
    bool needs_runtime_vil = false;

    if(!BuildGeometryDrivenVILConfig(material,
                                     geom,
                                     requested_vil,
                                     runtime_cfg,
                                     has_any,
                                     &reason,
                                     &needs_runtime_vil))
        return false;

    if(!has_any)
    {
        reason = "geometry_has_no_required_vab";
        return false;
    }

    if(needs_runtime_vil)
    {
        const uint32_t input_count = requested_vil->GetVertexAttribCount();
        const VertexInputFormat *vif = requested_vil->GetVIFList();

        for(uint32_t i = 0; i < input_count; ++i)
        {
            VAB *vab = geom->GetVAB(vif->attrib);
            if(vab && (vab->GetFormat() != vif->format || vab->GetStride() != vif->stride))
            {
                const char *vab_name = GetVertexAttribName(vif->attrib);
                GLogWarning(std::string(compat_log_tag ? compat_log_tag : "[PRIM_BIND_COMPAT]") +
                            " attrib=" + (vab_name ? vab_name : "") +
                            ", material=" + material_name +
                            ", vif_format=" + GetVulkanFormatName(vif->format) +
                            ", geo_format=" + GetVulkanFormatName(vab->GetFormat()) +
                            ", vif_stride=" + std::to_string(vif->stride) +
                            ", geo_stride=" + std::to_string(vab->GetStride()));
            }

            ++vif;
        }
    }

    if(!needs_runtime_vil)
    {
        effective_vil = requested_vil;
        owned_effective_vil = nullptr;
        return true;
    }

    owned_effective_vil = material->CreateVIL(&runtime_cfg);
    if(!owned_effective_vil)
    {
        reason = "runtime_vil_create_failed";
        return false;
    }

    effective_vil = owned_effective_vil;
    return true;
}

}

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
    ReleaseOwnedRuntimeVIL(vil, owned_runtime_vil, [&](VIL *runtime_vil)
    {
        if(material_template)
            material_template->Release(runtime_vil);
    });

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

bool Primitive::BindMaterialSlot(const PrimitiveMaterialSlot &slot,const char *source_tag)
{
    if (!source_tag)
    {
        static uint64_t s_bind_slot_untagged = 0;
        const uint64_t n = ++s_bind_slot_untagged;
        if (n <= 8u || ((n & (n - 1)) == 0))
        {
            GLogWarning("[BindMaterialSlot] untagged caller detected: prim='%s' total=%llu",
                        geometry ? geometry->GetName().c_str() : "(no-geometry)",
                        static_cast<unsigned long long>(n));
        }

#if defined(ULRE_BIND_MATERIAL_SLOT_REQUIRE_TAG) && (ULRE_BIND_MATERIAL_SLOT_REQUIRE_TAG!=0)
        GLogError("[BindMaterialSlot] blocked untagged caller (ULRE_BIND_MATERIAL_SLOT_REQUIRE_TAG=1): prim='%s'",
                  geometry ? geometry->GetName().c_str() : "(no-geometry)");
        return false;
#endif
    }

    // Note: slot.IsValid() requires mi_id>=0, but we accept mi_id=-1 for non-instanced material binding.
    // Check material_template instead to allow deferred MI slots that resolved material but couldn't allocate MI slot.
    if (!slot.material_template || !slot.vil || !geometry)
        return false;

    const std::string material_name = slot.material_template->GetName();
    const VIL *effective_vil = nullptr;
    VIL *owned_effective_vil = nullptr;
    std::string effective_reason;

    if(!ResolveEffectivePrimitiveVIL(geometry,
                                     slot.material_template,
                                     slot.vil,
                                     "[PRIM_BIND_COMPAT][DEFERRED]",
                                     material_name,
                                     effective_vil,
                                     owned_effective_vil,
                                     effective_reason))
    {
        GLogError(std::string("[FATAL ERROR] BindMaterialSlot can't satisfy Primitive input, MaterialTemplate(") +
                  material_name + ") reason(" + effective_reason + ")");
        DumpPrimitiveBindingDiagnostics(stderr,
                                       "[PRIM_BIND_DIAG]",
                                       geometry,
                                       slot.vil,
                                       material_name,
                                       effective_reason.c_str());
        if(owned_effective_vil)
            slot.material_template->Release(owned_effective_vil);
        return false;
    }

    const uint32_t required_vab_count = GetMaxBindingIndex(effective_vil) + 1;

    GeometryDataBuffer *geom_data_buffer = data_buffer;
    if(!geom_data_buffer || geom_data_buffer->vab_count != required_vab_count)
        geom_data_buffer = new GeometryDataBuffer(required_vab_count, geometry->GetIBO(), geometry->GetVDM());

    if(!geom_data_buffer->Update(geometry, effective_vil))
    {
        GLogError(std::string("[FATAL ERROR] BindMaterialSlot failed to update GeometryDataBuffer, MaterialTemplate(") +
                  material_name + ")");
        if(geom_data_buffer != data_buffer)
            delete geom_data_buffer;
        if(owned_effective_vil)
            slot.material_template->Release(owned_effective_vil);
        return false;
    }

    ReplaceRuntimeVILBinding(vil,
                             owned_runtime_vil,
                             effective_vil,
                             owned_effective_vil,
                             [&](VIL *runtime_vil)
                             {
                                 if(material_template)
                                     material_template->Release(runtime_vil);
                             });

    if(geom_data_buffer != data_buffer)
    {
        delete data_buffer;
        data_buffer = geom_data_buffer;
    }

    // Update all direct fields
    material_template    = slot.material_template;
    idd_manager_         = slot.idd_manager;         // P12
    idd_handle        = slot.idd_handle;   // P9
    mi_id                = slot.mi_id;
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
    idd_manager_      = slot.idd_manager;        // P12
    idd_handle     = slot.idd_handle;  // P9
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

    const std::string &mtl_name = slot.material_template->GetName();
    const VIL *effective_vil = nullptr;
    VIL *owned_effective_vil = nullptr;
    std::string effective_reason;

    if(!ResolveEffectivePrimitiveVIL(geom,
                                     slot.material_template,
                                     slot.vil,
                                     "[PRIM_BIND_COMPAT]",
                                     mtl_name,
                                     effective_vil,
                                     owned_effective_vil,
                                     effective_reason))
    {
        GLogError(std::string("[FATAL ERROR] DirectCreatePrimitive can't satisfy Primitive input, MaterialTemplate(") +
                  mtl_name + ") reason(" + effective_reason + ")");
        DumpPrimitiveBindingDiagnostics(stderr,
                           "[PRIM_BIND_DIAG]",
                           geom,
                           slot.vil,
                           mtl_name,
                           effective_reason.c_str());
        return nullptr;
    }

    const VIL *vil = effective_vil;
    const uint32_t input_count = vil->GetVertexAttribCount();

    if(geom->GetVABCount() < input_count)
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than MaterialTemplate, MaterialTemplate name: " + mtl_name);
        DumpPrimitiveBindingDiagnostics(stderr,
                                       "[PRIM_BIND_DIAG]",
                                       geom,
                                       vil,
                                       mtl_name,
                                       "vab_count_less_than_vil_input_count");
        if(owned_effective_vil)
            slot.material_template->Release(owned_effective_vil);
        return nullptr;
    }

    GeometryDataBuffer *geom_data_buffer = new GeometryDataBuffer(GetMaxBindingIndex(vil) + 1, geom->GetIBO(), geom->GetVDM());

    if(!geom_data_buffer->Update(geom, vil))
    {
        delete geom_data_buffer;
        if(owned_effective_vil)
            slot.material_template->Release(owned_effective_vil);
        return nullptr;
    }

    PrimitiveMaterialSlot effective_slot = slot;
    effective_slot.vil = effective_vil;

    Primitive *prim = new Primitive(geom, effective_slot, geom_data_buffer);
    prim->owned_runtime_vil = owned_effective_vil;
    return prim;
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
