#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/TransformComponent.h>

namespace hgl::ecs
{
    // PrimitiveRenderItem implementation
    PrimitiveRenderItem::PrimitiveRenderItem(
        EntityID ent_id,
        std::shared_ptr<TransformComponent> trans,
        std::shared_ptr<PrimitiveComponent> prim,
        std::shared_ptr<MaterialComponent> mat,
        ECSContext* ctx)
        : entity_id(ent_id)
        , context(ctx)
        , transform(trans)
        , primitiveComp(prim)
        , materialComp(mat)
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

    hgl::graph::ShaderProgram* PrimitiveRenderItem::GetShaderProgram() const
    {
        if (materialComp && materialComp->program)
            return materialComp->program;

        return primitiveComp ? primitiveComp->GetShaderProgram() : nullptr;
    }

    hgl::graph::Pipeline* PrimitiveRenderItem::GetPipeline() const
    {
        return primitiveComp ? primitiveComp->GetPipeline() : nullptr;
    }

    const hgl::graph::GeometryDataBuffer *PrimitiveRenderItem::GetGeometryDataBuffer() const
    {
        return primitiveComp ? primitiveComp->GetRuntimeGeometryDataBuffer() : nullptr;
    }

    const hgl::graph::GeometryDrawRange *PrimitiveRenderItem::GetGeometryDrawRange() const
    {
        return primitiveComp ? primitiveComp->GetRuntimeGeometryDrawRange() : nullptr;
    }

    TransformPolicySpec PrimitiveRenderItem::GetTransformPolicySpec() const
    {
        return primitiveComp ? primitiveComp->GetTransformPolicySpec() : TransformPolicySpec{};
    }

    PositionSourceSpec PrimitiveRenderItem::GetPositionSourceSpec() const
    {
        return primitiveComp ? primitiveComp->GetPositionSourceSpec() : PositionSourceSpec::MeshVertex;
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
