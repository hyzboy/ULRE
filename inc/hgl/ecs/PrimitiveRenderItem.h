#pragma once

#include<hgl/ecs/RenderItem.h>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class Material;
        class MaterialInstance;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class TransformComponent;
    class PrimitiveComponent;

    /**
     * PrimitiveRenderItem - specialized RenderItem for PrimitiveComponent
     * Similar to hgl::graph::DrawNodePrimitive in the old system
     */
    class PrimitiveRenderItem : public RenderItem
    {
    private:
        std::shared_ptr<Entity> entity;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitiveComp;
        glm::mat4 worldMatrix;

    public:
        PrimitiveRenderItem(
            std::shared_ptr<Entity> ent,
            std::shared_ptr<TransformComponent> trans,
            std::shared_ptr<PrimitiveComponent> prim);

        virtual ~PrimitiveRenderItem() = default;

        // Implement abstract interface
        std::shared_ptr<Entity> GetEntity() const override { return entity; }
        std::shared_ptr<TransformComponent> GetTransform() const override { return transform; }
        std::shared_ptr<RenderableComponent> GetRenderable() const override;
        glm::mat4 GetWorldMatrix() const override { return worldMatrix; }

        // PrimitiveComponent-specific accessors
        std::shared_ptr<PrimitiveComponent> GetPrimitiveComponent() const { return primitiveComp; }
        
        // Material batching interface
        hgl::graph::Primitive* GetPrimitive() const override;
        hgl::graph::MaterialInstance* GetMaterialInstance() const override;
        hgl::graph::Material* GetMaterial() const override;
        hgl::graph::Pipeline* GetPipeline() const override;

        // Update world matrix from transform
        void UpdateWorldMatrix();
    };
}//namespace hgl::ecs
