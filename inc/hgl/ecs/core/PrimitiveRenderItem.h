#pragma once

#include<hgl/ecs/core/RenderItem.h>

namespace hgl
{
    namespace graph
    {
        class ShaderProgram;
        class DescriptorBindingSet;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class Entity;
    class ECSContext;
    class TransformComponent;
    class PrimitiveComponent;
    class MaterialComponent;

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
        std::shared_ptr<MaterialComponent> materialComp;
        glm::mat4 worldMatrix;

    public:
        PrimitiveRenderItem(
            EntityID ent_id,
            std::shared_ptr<TransformComponent> trans,
            std::shared_ptr<PrimitiveComponent> prim,
            std::shared_ptr<MaterialComponent> mat = nullptr,
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
        std::shared_ptr<MaterialComponent> GetMaterialComponent() const { return materialComp; }

        // ShaderProgram batching interface
        hgl::graph::ShaderProgram* GetShaderProgram() const override;
        hgl::graph::Pipeline* GetPipeline() const override;
        const hgl::graph::GeometryDataBuffer *GetGeometryDataBuffer() const override;
        const hgl::graph::GeometryDrawRange *GetGeometryDrawRange() const override;
        TransformPolicySpec GetTransformPolicySpec() const override;
        PositionSourceSpec GetPositionSourceSpec() const override;

        // Update world matrix from transform
        void UpdateWorldMatrix();
    };
}//namespace hgl::ecs
