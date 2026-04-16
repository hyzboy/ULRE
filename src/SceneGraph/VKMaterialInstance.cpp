#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKResourceDomain.h>
#include<cstring>

namespace hgl::graph{

// ---------------------------------------------------------------------------
// Material::CreateMI — 旧路径（不涉及 ResourceDomain）
// ---------------------------------------------------------------------------

MaterialInstance *Material::CreateMI(const VIL *vil)
{
    // Phase 5: 旧路径统一通过懒初始化的 default_domain 分配 MI 槽位
    if(!default_domain && hasMI())
        default_domain = new ResourceDomain(GetShaderDataSchema(), 0);

    int mi_id = default_domain ? default_domain->AllocMISlot() : -1;

    auto *mi = new MaterialInstance(this, default_domain, vil ? vil : GetDefaultVIL(), mi_id);
    mi->InitMITLayout(texture_array_slot_flags);
    return mi;
}

MaterialInstance *Material::CreateMI(const VILConfig *vil_cfg)
{
    return CreateMI(CreateVIL(vil_cfg));
}

void Material::ReleaseMI(int mi_id)
{
    // Phase 5: 保留接口兼容性；通过 default_domain 代理释放
    if(mi_id < 0 || !default_domain) return;

    default_domain->FreeMISlot(mi_id);
}

void *Material::GetMIData(int id)
{
    // Phase 5: 通过 default_domain 代理访问
    if(!default_domain)
        return nullptr;

    return default_domain->GetMIData(id);
}

// ---------------------------------------------------------------------------
// MaterialInstanceData — constructors / destructor
// ---------------------------------------------------------------------------

/// 旧路径：domain = nullptr
MaterialInstance::MaterialInstance(Material *mtl, const VIL *v, const int id)
    : material(mtl), domain(nullptr), domain_id(0xFFFFFFFFu), vil(v), mi_id(id)
{
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));
}

/// Phase 1 新路径：经由 ResourceDomain 分配
MaterialInstance::MaterialInstance(Material *mtl, ResourceDomain *d, const VIL *v, const int id)
    : material(mtl), domain(d), domain_id(d ? d->GetDomainID() : 0xFFFFFFFFu), vil(v), mi_id(id)
{
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));
}

MaterialInstance::~MaterialInstance()
{
    if(domain)
        domain->FreeMISlot(mi_id);
    else
        material->ReleaseMI(mi_id);

    delete[] mit_packed;
}

// ---------------------------------------------------------------------------
// MaterialInstanceData — data access
// ---------------------------------------------------------------------------

void *MaterialInstance::GetMIData()
{
    if(domain)
        return domain->GetMIData(mi_id);

    return material->GetMIData(mi_id);
}

void MaterialInstance::WriteMIData(const void *data,const uint32 size)
{
    if(!data||!size||size>material->GetMIDataBytes())return;

    void *tp=GetMIData();

    if(tp)
        memcpy(tp,data,size);
}

void MaterialInstance::InitMITLayout(uint8_t slot_flags)
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

void MaterialInstance::SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer)
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if (off < 0) return;
    mit_packed[off] = layer;
}

uint32_t MaterialInstance::GetTextureArrayLayer(mtl::SamplerSlot slot) const
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if (off < 0) return 0;
    return mit_packed[off];
}

}//namespace hgl::graph

