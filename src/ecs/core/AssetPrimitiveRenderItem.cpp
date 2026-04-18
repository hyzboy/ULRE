#include <hgl/ecs/core/AssetPrimitiveRenderItem.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/graph/mesh/Primitive.h>
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
            state.material = state.binding_instance->GetShaderMaterialProgram();
            state.domain = state.binding_instance->GetDomain();
            state.domain_id = state.binding_instance->GetDomainID();
            state.vil = state.binding_instance->GetVIL();
            state.mi_id = state.binding_instance->GetMIID();
            state.preset = state.binding_instance->GetRenderPreset();

#ifdef _DEBUG
            assert(state.domain == state.binding_instance->GetDomain());
            assert(state.domain_id == state.binding_instance->GetDomainID());
            assert(state.vil == state.binding_instance->GetVIL());
            assert(state.mi_id == state.binding_instance->GetMIID());
            assert(state.preset == state.binding_instance->GetRenderPreset());
            assert(state.material == state.binding_instance->GetShaderMaterialProgram());
#endif
        }
        else
        {
            state.material = primitive->GetShaderMaterialProgram();
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
