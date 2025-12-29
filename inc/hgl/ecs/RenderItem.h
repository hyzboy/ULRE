#pragma once

#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformComponent.h>

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
    class World;
    class RenderableComponent;
    class PrimitiveComponent;

    /**
     * Base RenderItem class - abstract interface for rendering
     * Similar to hgl::graph::DrawNode in the old system
     */
    class RenderItem
    {
    public:
        uint32_t index = 0;                      // Index in batch
        uint32_t transform_version = 0;          // Transform version for dirty tracking
        uint32_t transform_index = 0;            // Transform index in buffer
        
        glm::vec3 worldPosition{};               // World space position
        float distanceToCamera = 0.0f;           // Distance to camera for sorting
        bool isVisible = true;                   // Visibility flag

        virtual ~RenderItem() = default;

        // Abstract interface - must be implemented by derived classes
        virtual std::shared_ptr<Entity> GetEntity() const = 0;
        virtual std::shared_ptr<TransformComponent> GetTransform() const = 0;
        virtual std::shared_ptr<RenderableComponent> GetRenderable() const = 0;
        virtual glm::mat4 GetWorldMatrix() const = 0;
        
        // For material batching support
        virtual hgl::graph::Primitive* GetPrimitive() const = 0;
        virtual hgl::graph::MaterialInstance* GetMaterialInstance() const = 0;
        virtual hgl::graph::Material* GetMaterial() const = 0;
        virtual hgl::graph::Pipeline* GetPipeline() const = 0;

        // Comparison for sorting
        virtual int Compare(const RenderItem& other) const;
    };

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
