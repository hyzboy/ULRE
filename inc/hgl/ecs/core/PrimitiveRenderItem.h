#pragma once

#include<hgl/ecs/core/RenderItem.h>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class Material;
        class MaterialInstance;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class Entity;
    class ECSContext;
    class TransformComponent;
    class PrimitiveComponent;

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
        // D-2: resolved MI from semantic resolve path, set by collector for this frame
        hgl::graph::MaterialInstance* resolved_mi = nullptr;

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

        // D-2: set the resolved MI for this frame (semantic path)
        void SetResolvedMI(hgl::graph::MaterialInstance* mi) { resolved_mi = mi; }
        hgl::graph::MaterialInstance* GetResolvedMI() const { return resolved_mi; }

        // Material batching interface
        hgl::graph::Primitive* GetPrimitive() const override;
        hgl::graph::MaterialInstance* GetMaterialInstance() const override;
        hgl::graph::Material* GetMaterial() const override;

        // Update world matrix from transform
        void UpdateWorldMatrix();
    };
}//namespace hgl::ecs

