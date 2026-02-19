#pragma once

#include<hgl/ecs/core/Component.h>
#include<memory>
#include<string>
#include<vector>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    class ECSContext;
    class Entity;
    class World;
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
        OBJECT_LOGGER

    private:
        std::shared_ptr<World> sub_world;
        Entity* owner_entity = nullptr;
        std::string asset_path;
        bool asset_binary = false;
        std::vector<EntityID> instanced_entity_ids;

        bool paused = false;
        bool tick_enabled = true;
        bool render_enabled = true;

    public:
        SubWorldComponent(const std::string& name = "SubWorld");
        ~SubWorldComponent() override;

    public:

        /// Initialize sub-world attached to parent context
        /// Shares the parent's TransformDataStorage for seamless parent-child relationships
        bool Initialize(ECSContext* parent_context);

        /// Get the sub-world object
        World* GetSubWorld() const { return sub_world.get(); }

        /// Get underlying sub-world ECS context
        ECSContext* GetSubContext() const;

        /// Get the sub-world object as shared_ptr (for ownership management)
        std::shared_ptr<World> GetSubWorldShared() const { return sub_world; }

        /// Update sub-world systems
        void UpdateSubWorld(float delta_time);

        /// Render sub-world
        void RenderSubWorld(graph::RenderCmdBuffer* cmd, float delta_time);

        /// Check if sub-world is initialized
        bool IsInitialized() const { return GetSubContext() != nullptr; }

        /// Destroy all entities in sub-world but keep the context
        void ClearSubWorld();

        /// Configure asset file for mix-in mode (no child ECSContext required)
        /// If path is set, OnAttach will instantiate this asset into parent ECS world.
        void SetAssetPath(const std::string& path, bool binary = false)
        {
            asset_path = path;
            asset_binary = binary;
        }

        const std::string& GetAssetPath() const { return asset_path; }
        bool IsAssetBinary() const { return asset_binary; }

        /// Instantiate asset entities into parent ECS context.
        bool InstantiateAssetToParent();

        /// Remove instantiated entities from parent ECS context.
        void ClearInstancedAssetEntities();

        const std::vector<EntityID>& GetInstancedEntityIDs() const { return instanced_entity_ids; }

        /// Pause both Tick and Render for this sub-world.
        /// Parent world can continue running while this sub-world is paused.
        void SetPaused(bool value) { paused = value; }
        bool IsPaused() const { return paused; }

        /// Fine-grained control for tick/render scheduling
        void SetTickEnabled(bool value) { tick_enabled = value; }
        bool IsTickEnabled() const { return tick_enabled; }

        void SetRenderEnabled(bool value) { render_enabled = value; }
        bool IsRenderEnabled() const { return render_enabled; }

    public:

        void OnAttach() override;
        void OnDetach() override;

    private:

        /// Setup visibility inheritance linkage
        void SetupVisibilityInheritance();
    };
}//namespace hgl::ecs

