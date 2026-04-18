#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKResourceDomain.h>
#include<cstring>

namespace hgl::graph{

// ---------------------------------------------------------------------------
// MaterialBindingInstanceData — constructors / destructor
// ---------------------------------------------------------------------------

/// Phase 1 新路径：经由 ResourceDomain 分配
MaterialBindingInstance::MaterialBindingInstance(ShaderMaterialProgram *mtl, ResourceDomain *d, const VIL *v, const int id)
    : material(mtl), domain(d), domain_id(d ? d->GetDomainID() : 0xFFFFFFFFu), vil(v), mi_id(id)
{
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));
}

MaterialBindingInstance::~MaterialBindingInstance()
{
    if(domain)
        domain->FreeMISlot(mi_id);

    delete[] mit_packed;
}

// ---------------------------------------------------------------------------
// MaterialBindingInstanceData — data access
// ---------------------------------------------------------------------------

void *MaterialBindingInstance::GetMIData()
{
    if(domain)
        return domain->GetMIData(mi_id);

    return nullptr;
}

void MaterialBindingInstance::WriteMIData(const void *data,const uint32 size)
{
    if(!data||!size) return;
    const uint32_t limit = domain ? domain->GetMIDataBytes() : 0;
    if(!limit||size>limit)return;


    void *tp=GetMIData();

    if(tp)
        memcpy(tp,data,size);
}

void MaterialBindingInstance::InitMITLayout(uint8_t slot_flags)
{
    delete[] mit_packed;
    mit_packed = nullptr;
    mit_packed_count = 0;
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));

    if (!slot_flags) return;

    uint32_t offset = 0;
    for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlot::RANGE_SIZE); ++s)
    {
        if (slot_flags & (1u << s))
        {
            mit_slot_offset[s] = static_cast<int8_t>(offset);
            ++offset;
        }
    }
    mit_packed_count = offset;
    mit_packed = new uint32_t[mit_packed_count];
    std::memset(mit_packed, 0, mit_packed_count * sizeof(uint32_t));
}

void MaterialBindingInstance::SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer)
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if (off < 0) return;
    mit_packed[off] = layer;
}

uint32_t MaterialBindingInstance::GetTextureArrayLayer(mtl::SamplerSlot slot) const
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if (off < 0) return 0;
    return mit_packed[off];
}

}//namespace hgl::graph

