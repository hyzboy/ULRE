#pragma once

#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class ShaderMaterialProgram;
        class MaterialBindingInstance;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class Entity;
    class ECSContext;
    class TransformComponent;

    /**
     * PrimitiveRenderItem - specialized RenderItem for PrimitiveComponent
     * Similar to hgl::graph::DrawNodePrimitive in the old system
     */
    class PrimitiveRenderItem : public RenderItem
    {
    private:
        EntityID entity_id;
        ECSContext* context = nullptr;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitiveComp;
        glm::mat4 worldMatrix;

    public:
        PrimitiveRenderItem(
            EntityID ent_id,
            std::shared_ptr<TransformComponent> trans,
            std::shared_ptr<PrimitiveComponent> prim,
            ECSContext* ctx = nullptr);

        virtual ~PrimitiveRenderItem() = default;

        // Implement abstract interface
        EntityID GetEntityID() const override { return entity_id; }
        Entity* GetEntity() const override;
        std::shared_ptr<TransformComponent> GetTransform() const override { return transform; }
        std::shared_ptr<RenderableComponent> GetRenderable() const override;
        glm::mat4 GetWorldMatrix() const override { return worldMatrix; }

        // PrimitiveComponent-specific accessors
        std::shared_ptr<PrimitiveComponent> GetPrimitiveComponent() const { return primitiveComp; }

        // ShaderMaterialProgram batching interface
        hgl::graph::Primitive* GetPrimitive() const override;
        // Compatibility overrides; runtime call sites should consume GetResolvedMaterialState().
        hgl::graph::MaterialBindingInstance* GetResolvedBindingInstance() const override;
        hgl::graph::ShaderMaterialProgram* GetShaderMaterialProgram() const override;
        // Unified runtime source of truth for material-related state.
        ResolvedMaterialState GetResolvedMaterialState() const override;

        // Update world matrix from transform
        void UpdateWorldMatrix();
    };
}//namespace hgl::ecs

