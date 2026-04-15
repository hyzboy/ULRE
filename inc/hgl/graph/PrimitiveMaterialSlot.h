#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include <hgl/graph/IDDHandle.h>

namespace hgl::graph
{

class MaterialTemplate;
class InstanceDataDomain;
class IDDManager;
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
 *   MaterialInstance::GetSlotID()      → slot_id
 *   MaterialInstance::GetVIL()         → vil
 *   MaterialInstance::GetRenderPreset()→ preset
 *
 * Lifetime: the slot is valid only as long as the domain slot (slot_id) is live.
 * Ownership (domain slot lifecycle) stays inside MaterialAssetRegistry.
 */
struct PrimitiveMaterialSlot
{
    MaterialTemplate        *material_template = nullptr;
    InstanceDataDomain  *    domain            = nullptr;
    IDDHandle                idd_handle        = {};          // P9: handle-based identity alongside raw ptr cache
    IDDManager              *idd_manager       = nullptr;     // P12: for Primitive data-access delegation
    int                      slot_id           = -1;
    const VIL               *vil               = nullptr;
    GraphicsPipelinePreset   preset            = GraphicsPipelinePreset::Solid3D;
    uint8_t                  texture_array_slot_flags = 0;
    const uint32_t          *mit_data          = nullptr;
    uint32_t                 mit_data_count    = 0;
    mtl::MaterialPreset      material_preset   = mtl::MaterialPreset::Standard;

    /// Init-time check: domain slot allocated, data can be written.
    bool HasData() const
    {
        return domain != nullptr;
    }

    /// Render-time check: full binding state ready for pipeline creation.
    bool IsRenderable() const
    {
        return material_template != nullptr
            && domain != nullptr
            && vil != nullptr;
    }

    /// Legacy alias — equivalent to IsRenderable().
    bool IsValid() const
    {
        return IsRenderable();
    }
};

} // namespace hgl::graph
