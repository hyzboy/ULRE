#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

namespace hgl::graph
{

class MaterialTemplate;
class MaterialResourceDomain;
class VertexInputLayout;
using VIL = VertexInputLayout;

/**
 * PrimitiveMaterialSlot — lightweight POD that carries the resolved binding data
 * a Primitive needs, replacing MaterialInstance* in the ECS hot path.
 *
 * MaterialAssetRegistry::ResolveMI() returns this instead of MaterialInstance*.
 * Primitive::BindMaterialSlot() consumes it.
 *
 * Scope: this slot only carries resolved render binding state.
 * It intentionally does NOT carry RuntimeMaterialRequest.
 *
 * The slot's fields correspond 1-to-1 with the MI accessors they replace:
 *   MaterialInstance::GetMaterial()    → material_template
 *   MaterialInstance::GetDomain()      → domain
 *   MaterialInstance::GetMIID()        → mi_id
 *   MaterialInstance::GetVIL()         → vil
 *   MaterialInstance::GetRenderPreset()→ preset
 *
 * Lifetime: the slot is valid only as long as the domain slot (mi_id) is live.
 * Ownership (domain slot lifecycle) stays inside MaterialAssetRegistry.
 */
struct PrimitiveMaterialSlot
{
    MaterialTemplate        *material_template = nullptr;
    MaterialResourceDomain  *domain            = nullptr;
    int                      mi_id             = -1;
    const VIL               *vil               = nullptr;
    GraphicsPipelinePreset   preset            = GraphicsPipelinePreset::Solid3D;
    uint8_t                  texture_array_slot_flags = 0;
    const uint32_t          *mit_data          = nullptr;
    uint32_t                 mit_data_count    = 0;
    mtl::MaterialPreset      material_preset   = mtl::MaterialPreset::Standard;

    bool IsValid() const { return material_template != nullptr && mi_id >= 0; }
};

} // namespace hgl::graph
