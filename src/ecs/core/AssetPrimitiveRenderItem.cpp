#include <hgl/ecs/core/AssetPrimitiveRenderItem.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include <hgl/vk/VKShaderMaterialProgram.h>
#include <hgl/vk/VKMaterialBindingInstance.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include <cassert>

namespace hgl::ecs
{
    AssetPrimitiveRenderItem::AssetPrimitiveRenderItem(
        EntityID                            ent_id,
        std::shared_ptr<TransformComponent> trans,
        graph::Primitive*                   prim,
        graph::AssetWorldDef::ID            aid,
        ECSContext*                         ctx)
        : entity_id(ent_id)
        , context(ctx)
        , transform(std::move(trans))
        , primitive(prim)
        , asset_id(aid)
        , world_matrix(1.0f)
    {
        if (transform)
        {
            world_matrix  = transform->GetWorldMatrix();
            worldPosition = transform->GetWorldPosition();
        }
    }

    Entity* AssetPrimitiveRenderItem::GetEntity() const
    {
        if (!context || !entity_id.IsValid())
            return nullptr;
        return context->GetEntity(entity_id);
    }

    graph::MaterialBindingInstance* AssetPrimitiveRenderItem::GetResolvedBindingInstance() const
    {
        return GetResolvedMaterialState().binding_instance;
    }

    graph::ShaderMaterialProgram* AssetPrimitiveRenderItem::GetShaderMaterialProgram() const
    {
        return GetResolvedMaterialState().material;
    }

    RenderItem::ResolvedMaterialState AssetPrimitiveRenderItem::GetResolvedMaterialState() const
    {
        ResolvedMaterialState state{};
        state.preset = hgl::graph::GraphicsPipelinePreset::Solid3D;

        if (!primitive)
            return state;

        state.binding_instance = primitive->GetResolvedBindingInstance();

        if (state.binding_instance)
        {
            // Stage-5: VIL is now sourced from Primitive (stored at construction in Stage-4).
            // material still comes from MI until MI.material field is removed in a later pass.
            state.material  = graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(state.binding_instance);
            state.vil       = primitive->GetVIL();
            state.domain    = graph::MaterialBindingInstanceInternalAccess::GetDomain(state.binding_instance);
            state.domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(state.binding_instance);

            state.mi_id  = state.binding_instance->GetMIID();
            state.preset = state.binding_instance->GetRenderPreset();

#ifdef _DEBUG
            assert(state.material  == graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(state.binding_instance));
            assert(state.domain    == graph::MaterialBindingInstanceInternalAccess::GetDomain(state.binding_instance));
            assert(state.domain_id == graph::MaterialBindingInstanceInternalAccess::GetDomainID(state.binding_instance));

            assert(state.mi_id  == state.binding_instance->GetMIID());
            assert(state.preset == state.binding_instance->GetRenderPreset());
#endif
        }

        return state;
    }

    void AssetPrimitiveRenderItem::UpdateWorldMatrix()
    {
        if (transform)
        {
            world_matrix  = transform->GetWorldMatrix();
            worldPosition = transform->GetWorldPosition();
        }
    }

}//namespace hgl::ecs
