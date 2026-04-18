#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<cassert>

namespace hgl::ecs
{
    // PrimitiveRenderItem implementation
    PrimitiveRenderItem::PrimitiveRenderItem(
        EntityID ent_id,
        std::shared_ptr<TransformComponent> trans,
        std::shared_ptr<PrimitiveComponent> prim,
        ECSContext* ctx)
        : entity_id(ent_id)
        , context(ctx)
        , transform(trans)
        , primitiveComp(prim)
        , worldMatrix(1.0f)
    {
        if (transform)
        {
            worldMatrix = transform->GetWorldMatrix();
            worldPosition = transform->GetWorldPosition();
        }
    }

    Entity* PrimitiveRenderItem::GetEntity() const
    {
        if (!context || !entity_id.IsValid())
            return nullptr;
        return context->GetEntity(entity_id);
    }

    std::shared_ptr<RenderableComponent> PrimitiveRenderItem::GetRenderable() const
    {
        return std::static_pointer_cast<RenderableComponent>(primitiveComp);
    }

    hgl::graph::Primitive* PrimitiveRenderItem::GetPrimitive() const
    {
        return primitiveComp ? primitiveComp->GetPrimitive() : nullptr;
    }

    hgl::graph::MaterialBindingInstance* PrimitiveRenderItem::GetResolvedBindingInstance() const
    {
        return GetResolvedMaterialState().binding_instance;
    }

    hgl::graph::ShaderMaterialProgram* PrimitiveRenderItem::GetShaderMaterialProgram() const
    {
        return GetResolvedMaterialState().material;
    }

    RenderItem::ResolvedMaterialState PrimitiveRenderItem::GetResolvedMaterialState() const
    {
        if (!primitiveComp)
            return RenderItem::ResolvedMaterialState{};

        const auto comp_state = primitiveComp->ResolveEffectiveMaterialState();

        RenderItem::ResolvedMaterialState state{};
        state.binding_instance = comp_state.binding_instance;
        state.material = comp_state.material;
        state.domain = comp_state.domain;
        state.domain_id = comp_state.domain_id;
        state.vil = comp_state.vil;
        state.mi_id = comp_state.mi_id;
        state.preset = comp_state.preset;

    #ifdef _DEBUG
        if (state.binding_instance)
        {
            assert(state.domain == state.binding_instance->GetDomain());
            assert(state.domain_id == state.binding_instance->GetDomainID());

        #if ULRE_PRIMITIVE_USE_LEGACY_MI_GETTER
            assert(state.vil == state.binding_instance->GetVIL());
        #endif

            assert(state.mi_id == state.binding_instance->GetMIID());
            assert(state.preset == state.binding_instance->GetRenderPreset());

        #if ULRE_PRIMITIVE_USE_LEGACY_MI_GETTER
            if (state.material)
                assert(state.material == state.binding_instance->GetShaderMaterialProgram());
        #endif
        }
    #endif

        return state;
    }

    void PrimitiveRenderItem::UpdateWorldMatrix()
    {
        if (transform)
        {
            worldMatrix = transform->GetWorldMatrix();
            worldPosition = transform->GetWorldPosition();
        }
    }
}//namespace hgl::ecs

