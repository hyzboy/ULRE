#include<hgl/ecs/core/RenderItem.h>
#include<hgl/graph/mesh/Primitive.h>
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
            state.domain    = state.binding_instance->GetDomain();
            state.domain_id = state.binding_instance->GetDomainID();

            state.mi_id     = state.binding_instance->GetMIID();
            state.preset    = state.binding_instance->GetRenderPreset();
            state.material  = GetShaderMaterialProgram();

#ifdef _DEBUG
            // Consistency: re-query each field and confirm it matches what we just wrote.
            // If any assert fires it means the MI returned different values in a second call —
            // that would be a MI-internal bug, not a pipeline bug.
            assert(state.domain    == state.binding_instance->GetDomain());
            assert(state.domain_id == state.binding_instance->GetDomainID());

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

