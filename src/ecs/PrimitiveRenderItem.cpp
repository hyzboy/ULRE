#include<hgl/ecs/PrimitiveRenderItem.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/RenderableComponent.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/pipeline/VKPipeline.h>

namespace hgl::ecs
{
    // PrimitiveRenderItem implementation
    PrimitiveRenderItem::PrimitiveRenderItem(
        std::shared_ptr<Entity> ent,
        std::shared_ptr<TransformComponent> trans,
        std::shared_ptr<PrimitiveComponent> prim)
        : entity(ent)
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
