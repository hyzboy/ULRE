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
    struct EntityMaterialBinding
    {
        bool valid = false;
        hgl::graph::MaterialTemplate* material_template = nullptr;
        hgl::graph::MaterialResourceDomain* domain = nullptr;
        int mi_id = -1;
        const hgl::graph::VIL* vil = nullptr;
        const uint32_t* mit_data = nullptr;
        uint32_t mit_count = 0;

        bool IsDrawBindingValid() const
        {
            return valid && material_template != nullptr && domain != nullptr;
        }

        bool HasSnapshotSignal() const
        {
            return material_template != nullptr || domain != nullptr || mi_id >= 0;
        }

        void Clear()
        {
            valid = false;
            material_template = nullptr;
            domain = nullptr;
            mi_id = -1;
            vil = nullptr;
            mit_data = nullptr;
            mit_count = 0;
        }

        void Assign(const hgl::graph::PrimitiveMaterialSlot& slot)
        {
            valid = slot.material_template != nullptr && slot.domain != nullptr;
            material_template = slot.material_template;
            domain = slot.domain;
            mi_id = slot.mi_id;
            vil = slot.vil;
            mit_data = slot.mit_data;
            mit_count = slot.mit_data_count;
        }
    };

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

        // Phase C: explicit entity-level material binding owned by RenderItem.
        EntityMaterialBinding entity_material_binding;

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

        const EntityMaterialBinding& GetEntityMaterialBinding() const { return entity_material_binding; }
        bool HasEntityMaterialBinding() const { return entity_material_binding.IsDrawBindingValid(); }
        bool HasEntityMaterialBindingSignal() const { return entity_material_binding.HasSnapshotSignal(); }

        void SetResolvedMaterialSlot(const hgl::graph::PrimitiveMaterialSlot& slot)
        {
            entity_material_binding.Assign(slot);
        }

        void ClearResolvedMaterialSlot()
        {
            entity_material_binding.Clear();
        }

        // Comparison for sorting
        virtual int Compare(const RenderItem& other) const;
    };

}//namespace hgl::ecs

