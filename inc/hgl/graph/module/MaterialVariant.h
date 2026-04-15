#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include <hgl/graph/IDDHandle.h>

namespace hgl::graph
{

class MaterialTemplate;
class VertexInputLayout;
using VIL = VertexInputLayout;

/// MaterialVariant — pure variant resolution result, no MI slot allocation.
///
/// Returned by MaterialAssetRegistry::ResolveVariant().
/// Carries the resolved material template, VIL, pipeline preset, and domain handle
/// needed to assemble a PrimitiveMaterialSlot without allocating any MI slot.
struct MaterialVariant
{
    MaterialTemplate       *material_template = nullptr;
    const VIL              *vil               = nullptr;
    GraphicsPipelinePreset  preset            = GraphicsPipelinePreset::Solid3D;
    mtl::MaterialPreset     material_preset   = mtl::MaterialPreset::Standard;
    IDDHandle               idd_handle;
    uint8_t                 texture_array_slot_flags = 0;

    bool IsValid() const
    {
        return material_template != nullptr
            && vil != nullptr;
    }
};

} // namespace hgl::graph
