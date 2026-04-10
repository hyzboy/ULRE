#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/graph/PrimitiveMaterialSlot.h>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class MaterialTemplate;
        class MaterialResourceDomain;
        class VertexInputLayout;
        using VIL = VertexInputLayout;
    }

    namespace ecs
    {
        class Entity;
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

        // Optional per-item resolved material slot snapshot.
        // When present, ECS upload/batching can use this entity-level state
        // instead of relying on mutable/shared Primitive state.
        bool resolved_slot_valid = false;
        hgl::graph::MaterialTemplate* resolved_material_template = nullptr;
        hgl::graph::MaterialResourceDomain* resolved_domain = nullptr;
        int resolved_mi_id = -1;
        const hgl::graph::VIL* resolved_vil = nullptr;
        const uint32_t* resolved_mit_data = nullptr;
        uint32_t resolved_mit_count = 0;

        virtual ~RenderItem() = default;

        // Abstract interface - returns EntityID and entity pointer
        virtual EntityID GetEntityID() const = 0;
        virtual Entity* GetEntity() const = 0;
        virtual std::shared_ptr<TransformComponent> GetTransform() const = 0;
        virtual std::shared_ptr<RenderableComponent> GetRenderable() const = 0;
        virtual glm::mat4 GetWorldMatrix() const = 0;

        // For material batching support
        virtual hgl::graph::Primitive* GetPrimitive() const = 0;
        virtual hgl::graph::MaterialTemplate* GetMaterial() const = 0;

        void SetResolvedMaterialSlot(const hgl::graph::PrimitiveMaterialSlot& slot)
        {
            // Phase B: drawing path only needs material+domain. Instance-indexed
            // paths must still gate on resolved_mi_id >= 0 at their own callsites.
            resolved_slot_valid = slot.material_template != nullptr && slot.domain != nullptr;
            resolved_material_template = slot.material_template;
            resolved_domain = slot.domain;
            resolved_mi_id = slot.mi_id;
            resolved_vil = slot.vil;
            resolved_mit_data = slot.mit_data;
            resolved_mit_count = slot.mit_data_count;
        }

        void ClearResolvedMaterialSlot()
        {
            resolved_slot_valid = false;
            resolved_material_template = nullptr;
            resolved_domain = nullptr;
            resolved_mi_id = -1;
            resolved_vil = nullptr;
            resolved_mit_data = nullptr;
            resolved_mit_count = 0;
        }

        // Comparison for sorting
        virtual int Compare(const RenderItem& other) const;
    };

}//namespace hgl::ecs

