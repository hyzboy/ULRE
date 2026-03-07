#pragma once

#include<hgl/ecs/core/Component.h>
#include<memory>
#include<string>
#include<vector>
#include<cstdint>
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
    enum class SubWorldMode : uint8_t
    {
        SharedContext = 0,
        IsolatedContext = 1
    };

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
        SubWorldMode mode = SubWorldMode::SharedContext;
        bool render_shared = true;
        bool logic_isolated = false;
        uint64_t subscene_id = 0;
        EntityID root_entity_id;
        std::string asset_path;
        bool asset_binary = false;
        std::vector<EntityID> instanced_entity_ids;

        bool paused = false;
        bool tick_enabled = true;
        bool render_enabled = true;

    public:
        SubWorldComponent(const std::string& name = "SubWorld");
        SubWorldComponent(SubWorldMode init_mode, const std::string& name = "SubWorld");
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

        void SetMode(SubWorldMode m);
        SubWorldMode GetMode() const { return mode; }

        void SetRenderShared(bool value);
        bool IsRenderShared() const { return render_shared; }

        void SetLogicIsolated(bool value);
        bool IsLogicIsolated() const { return logic_isolated; }

        uint64_t GetSubsceneID() const { return subscene_id; }
        EntityID GetRootEntityID() const { return root_entity_id; }

        /// Update sub-world systems
        void UpdateSubWorld(float delta_time);

        /// Prepare sub-world for rendering: run Collect+Batch+Upload phases
        /// MUST be called before the parent's BeginRenderPass (outside render pass).
        void PrepareSubWorld(float delta_time);

        /// Issue sub-world draw commands into an already-open render pass.
        /// PrepareSubWorld() MUST have been called first this frame.
        void DrawSubWorld(graph::RenderCmdBuffer* cmd, float delta_time);

        /// Render sub-world (legacy: Prepare+Draw in one call — only safe when
        /// the child context uses CPUVisible buffers, i.e. on ReBAR hardware).
        /// Prefer PrepareSubWorld() + DrawSubWorld() for correct non-ReBAR support.
        void RenderSubWorld(graph::RenderCmdBuffer* cmd, float delta_time);

        /// Check if sub-world is initialized
        bool IsInitialized() const;

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

        void SyncPolicyFromMode();
        void SyncModeFromPolicy();
        bool RequiresLocalContext() const { return logic_isolated || !render_shared; }

        /// Setup visibility inheritance linkage
        void SetupVisibilityInheritance();
    };
}//namespace hgl::ecs

