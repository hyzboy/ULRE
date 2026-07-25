#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialProgram.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/pipeline/VKPipeline.h>

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
        if (primitiveComp && primitiveComp->HasMaterialRecipe())
            return nullptr;

        return primitiveComp ? primitiveComp->GetMaterialInstance() : nullptr;
    }

    hgl::graph::DescriptorBindingSet* PrimitiveRenderItem::GetDescriptorBindingSet() const
    {
        if (primitiveComp)
        {
            if (primitiveComp->HasMaterialRecipe())
                return nullptr;

            if (auto *prim_dbs = primitiveComp->GetDescriptorBindingSet())
                return prim_dbs;
        }
        return nullptr;
    }

    hgl::graph::MaterialProgram* PrimitiveRenderItem::GetMaterialProgram() const
    {
        const bool uses_recipe_runtime = (primitiveComp && primitiveComp->HasMaterialRecipe());

        if (auto *entity = GetEntity())
        {
            auto material_comp = entity->GetComponent<MaterialComponent>();
            if (material_comp && material_comp->program)
                return material_comp->program;
        }

        if (uses_recipe_runtime)
            return nullptr;

        return primitiveComp ? primitiveComp->GetMaterialProgram() : nullptr;
    }

    hgl::graph::Pipeline* PrimitiveRenderItem::GetPipeline() const
    {
        return primitiveComp ? primitiveComp->GetPipeline() : nullptr;
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
