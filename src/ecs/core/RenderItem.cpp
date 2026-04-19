#include<hgl/ecs/core/RenderItem.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<cassert>

namespace hgl::ecs
{
    RenderItem::ResolvedMaterialState RenderItem::GetResolvedMaterialState() const
    {
        ResolvedMaterialState state{};
        state.preset = hgl::graph::GraphicsPipelinePreset::Solid3D;

        state.binding_instance = GetResolvedBindingInstance();

        if (state.binding_instance)
        {
            // Stage-5: material/vil are no longer sourced from MI fields.
            // Subclasses that override GetResolvedMaterialState() populate them
            // via the resolver cache (PrimitiveRenderItem) or primitive storage
            // (AssetPrimitiveRenderItem). The base-class fallback leaves them null.
            state.domain    = hgl::graph::MaterialBindingInstanceInternalAccess::GetDomain(state.binding_instance);
            state.domain_id = hgl::graph::MaterialBindingInstanceInternalAccess::GetDomainID(state.binding_instance);

            state.mi_id     = state.binding_instance->GetMIID();
            state.preset    = state.binding_instance->GetRenderPreset();

#ifdef _DEBUG
            assert(state.domain    == hgl::graph::MaterialBindingInstanceInternalAccess::GetDomain(state.binding_instance));
            assert(state.domain_id == hgl::graph::MaterialBindingInstanceInternalAccess::GetDomainID(state.binding_instance));

            assert(state.mi_id     == state.binding_instance->GetMIID());
            assert(state.preset    == state.binding_instance->GetRenderPreset());
#endif
        }
        return state;
    }

    // RenderItem base class implementation
    int RenderItem::Compare(const RenderItem& other) const
    {
        // Compare by primitive geometry first (for batching)
        auto* prim1 = GetPrimitive();
        auto* prim2 = other.GetPrimitive();

        if (prim1 && prim2)
        {
            // Compare geometry pointers for batching efficiency
            auto* geom1 = prim1->GetGeometry();
            auto* geom2 = prim2->GetGeometry();

            if (geom1 < geom2) return -1;
            if (geom1 > geom2) return 1;
        }

        // Then compare by distance to camera
        float diff = other.distanceToCamera - distanceToCamera;
        if (diff > 0.0f) return 1;
        if (diff < 0.0f) return -1;

        return 0;
    }
}//namespace hgl::ecs

