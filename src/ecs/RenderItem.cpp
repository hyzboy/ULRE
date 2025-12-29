#include<hgl/ecs/RenderItem.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/RenderableComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/pipeline/VKPipeline.h>

namespace hgl::ecs
{
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
