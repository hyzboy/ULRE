// Compatibility implementation retained temporarily for legacy callers.

#include <cstring>

#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKMaterialResourceDomain.h>
#include <hgl/graph/module/MaterialManager.h>

namespace hgl::graph {

MaterialInstance::MaterialInstance()
{
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));
}

MaterialInstance::MaterialInstance(const PrimitiveMaterialSlot &slot)
    : material(slot.material_template), vil(slot.vil), mi_id(slot.mi_id), render_preset(slot.preset), material_preset(slot.material_preset)
    // domain_resolver/domain_id/domain_generation left at defaults (0) — this ctor has no manager context
    // and therefore cannot safely claim slot ownership.
{
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));

    if (slot.texture_array_slot_flags != 0)
    {
        InitMITLayout(slot.texture_array_slot_flags);

        if (slot.mit_data && mit_packed && slot.mit_data_count > 0)
        {
            const uint32_t copy_count = (slot.mit_data_count < mit_packed_count) ? slot.mit_data_count : mit_packed_count;
            std::memcpy(mit_packed, slot.mit_data, copy_count * sizeof(uint32_t));
        }
    }
}

MaterialInstance::~MaterialInstance()
{
    if (owns_slot && mi_id >= 0)
    {
        if (auto *domain = GetDomain())
            domain->FreeMISlot(mi_id);
    }

    delete[] mit_packed;
}

// Phase E: resolve domain pointer through the manager's domain table
MaterialResourceDomain *MaterialInstance::GetDomain() const
{
    if (!domain_resolver || domain_id == 0)
        return nullptr;
    return domain_resolver->ResolveDomain(domain_id, domain_generation);
}

void *MaterialInstance::GetMIData()
{
    auto *d = GetDomain();
    if (!d || mi_id < 0)
        return nullptr;

    return d->GetMIData(mi_id);
}

void MaterialInstance::WriteMIData(const void *data, uint32_t size)
{
    if (!data || size == 0)
        return;

    void *dst = GetMIData();
    if (!dst)
        return;

    std::memcpy(dst, data, size);
}

void MaterialInstance::InitMITLayout(uint8_t slot_flags)
{
    delete[] mit_packed;
    mit_packed = nullptr;
    mit_packed_count = 0;
    std::memset(mit_slot_offset, -1, sizeof(mit_slot_offset));

    uint32_t offset = 0;

    for (uint8_t s = 0; s < uint8_t(mtl::SamplerSlotCount); ++s)
    {
        if ((slot_flags & (1u << s)) != 0)
        {
            mit_slot_offset[s] = static_cast<int8_t>(offset);
            ++offset;
        }
    }

    mit_packed_count = offset;

    if (mit_packed_count > 0)
    {
        mit_packed = new uint32_t[mit_packed_count];
        std::memset(mit_packed, 0, mit_packed_count * sizeof(uint32_t));
    }
}

void MaterialInstance::SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer)
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if (off < 0 || !mit_packed)
        return;

    mit_packed[off] = layer;
}

uint32_t MaterialInstance::GetTextureArrayLayer(mtl::SamplerSlot slot) const
{
    const int8_t off = mit_slot_offset[uint8_t(slot)];
    if (off < 0 || !mit_packed)
        return 0;

    return mit_packed[off];
}

} // namespace hgl::graph
