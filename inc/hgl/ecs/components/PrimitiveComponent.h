#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/graph/module/RuntimeMaterialRequest.h>
#include<glm/glm.hpp>

// Forward declarations to avoid heavy includes
namespace hgl
{
    namespace math
    {
        class AABB;
    }

    namespace graph
    {
        class Primitive;
        class MaterialTemplate;
        class MaterialInstance;  // Legacy/gizmo overlay path
    }
}

namespace hgl::ecs
{
    /**
     * PrimitiveComponent - Renderable component for static mesh rendering
     *
     * Manages a single Primitive (geometry + material) for rendering.
     * Derived from RenderableComponent to provide rendering capabilities.
     *
     * Features:
     * - Holds reference to hgl::graph::Primitive
     * - Supports MaterialInstanceData override
     * - Provides access to MaterialTemplate, GraphicsPipeline, and AABB data
     * - Compatible with RenderCollector for batched rendering
     */
    class PrimitiveComponent : public RenderableComponent
    {
    private:

        hgl::graph::Primitive* primitive;              // The primitive to render (not owned)
        hgl::graph::SemanticMaterialId semanticMaterialId = 0; // 0 = unset

    public:

        explicit PrimitiveComponent(const std::string& name = "Primitive")
            : RenderableComponent(name)
            , primitive(nullptr)
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetPrimitive(hgl::graph::Primitive* prim);
        hgl::graph::Primitive* GetPrimitive() const { return primitive; }

        // Semantic material id (Phase 2 path)
        void SetSemanticMaterial(hgl::graph::SemanticMaterialId id) { semanticMaterialId = id; }
        hgl::graph::SemanticMaterialId GetSemanticMaterial() const { return semanticMaterialId; }
        bool HasSemanticMaterial() const { return semanticMaterialId != 0; }

        // MaterialTemplate access
        hgl::graph::MaterialTemplate* GetMaterial() const;

        // Material override — legacy/gizmo path (calls primitive->BindMaterialSlot internally)
        void SetOverrideMaterial(hgl::graph::MaterialInstance* mi);

        // Bounding volume
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        // Rendering capability check
        bool CanRender() const;

    public:

        // Override RenderableComponent::Render
        void Render(const glm::mat4& worldMatrix) override;

        // Component lifecycle
        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}//namespace hgl::ecs


