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

}//namespace hgl::ecs
