#pragma once

#include<hgl/ecs/core/Component.h>
#include<memory>

namespace hgl::ecs
{
    class ECSContext;
    class Entity;
}

namespace hgl::graph
{
    class RenderCmdBuffer;
}

namespace hgl::ecs
{
    /**
     * SubWorldComponent - Hierarchical ECS worlds
     * 
     * Allows embedding a complete ECS world as a component.
     * The sub-world shares the parent's TransformDataStorage for seamless
     * cross-context parent-child relationships.
     * 
     * Features:
     * - Multi-level nesting support (House → Room → Furniture)
     * - Transform updates cascade from parent World to SubWorld
     * - Visibility inheritance (parent hidden → children hidden)
     * - Automatic cleanup on destruction
     * 
     * Usage:
     *   auto house = mainWorld->CreateEntity("House");
     *   auto sub = house->AddComponent<SubWorldComponent>();
     *   auto door = sub->GetSubWorld()->CreateEntity("Door");
     *   door->AddComponent<Transform>()->SetParent(house->GetID());
     */
    class SubWorldComponent : public Component
    {
    private:
        std::shared_ptr<ECSContext> sub_world;
        Entity* owner_entity = nullptr;

    public:
        SubWorldComponent(const std::string& name = "SubWorld");
        ~SubWorldComponent() override;

    public:
        
        /// Initialize sub-world attached to parent context
        /// Shares the parent's TransformDataStorage for seamless parent-child relationships
        bool Initialize(ECSContext* parent_context);

        /// Get the sub-world context
        ECSContext* GetSubWorld() const { return sub_world.get(); }

        /// Get the sub-world context as shared_ptr (for ownership management)
        std::shared_ptr<ECSContext> GetSubWorldShared() const { return sub_world; }

        /// Update sub-world systems
        void UpdateSubWorld(float delta_time);

        /// Render sub-world
        void RenderSubWorld(graph::RenderCmdBuffer* cmd, float delta_time);

        /// Check if sub-world is initialized
        bool IsInitialized() const { return sub_world != nullptr; }

        /// Destroy all entities in sub-world but keep the context
        void ClearSubWorld();

    public:

        void OnAttach() override;
        void OnDetach() override;

    private:

        /// Setup visibility inheritance linkage
        void SetupVisibilityInheritance();
    };
}//namespace hgl::ecs

