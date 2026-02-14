#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/pipeline/VKPipeline.h>

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

    hgl::graph::MaterialInstance* PrimitiveRenderItem::GetMaterialInstance() const
    {
        return primitiveComp ? primitiveComp->GetMaterialInstance() : nullptr;
    }

    hgl::graph::Material* PrimitiveRenderItem::GetMaterial() const
    {
        return primitiveComp ? primitiveComp->GetMaterial() : nullptr;
    }

    hgl::graph::Pipeline* PrimitiveRenderItem::GetPipeline() const
    {
        return primitiveComp ? primitiveComp->GetPipeline() : nullptr;
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

