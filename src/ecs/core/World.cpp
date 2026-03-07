#include<hgl/ecs/core/World.h>
#include<hgl/ecs/components/SubWorldComponent.h>

namespace
{
    struct SubWorldDispatchStats
    {
        uint32_t shared_count = 0;
        uint32_t isolated_count = 0;
        uint32_t dispatched_count = 0;
    };

    template<typename Fn>
    SubWorldDispatchStats DispatchLocalLogicSubWorldComponents(hgl::ecs::ECSContext* context, Fn&& fn)
    {
        SubWorldDispatchStats stats;

        if (!context)
            return stats;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (!sub_world)
                continue;

            if (!sub_world->IsLogicIsolated())
            {
                ++stats.shared_count;
                continue;
            }

            ++stats.isolated_count;
            fn(*sub_world);
            ++stats.dispatched_count;
        }

        return stats;
    }

    template<typename Fn>
    SubWorldDispatchStats DispatchLocalRenderSubWorldComponents(hgl::ecs::ECSContext* context, Fn&& fn)
    {
        SubWorldDispatchStats stats;

        if (!context)
            return stats;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (!sub_world)
                continue;

            if (sub_world->IsRenderShared())
            {
                ++stats.shared_count;
                continue;
            }

            ++stats.isolated_count;
            fn(*sub_world);
            ++stats.dispatched_count;
        }

        return stats;
    }

    template<typename Fn>
    SubWorldDispatchStats DispatchHybridBridgeSubWorldComponents(hgl::ecs::ECSContext* context, Fn&& fn)
    {
        SubWorldDispatchStats stats;

        if (!context)
            return stats;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (!sub_world)
                continue;

            const bool is_hybrid_bridge = sub_world->IsLogicIsolated() && sub_world->IsRenderShared();
            if (!is_hybrid_bridge)
            {
                ++stats.shared_count;
                continue;
            }

            ++stats.isolated_count;
            fn(*sub_world);
            ++stats.dispatched_count;
        }

        return stats;
    }

    void SyncChildFrameIndex(hgl::ecs::ECSContext* parent_context, hgl::ecs::ECSContext* child_context)
    {
        if (!parent_context || !child_context)
            return;

        const uint32_t parent_frame = parent_context->GetFrameIndex();
        const uint32_t child_frame = child_context->GetFrameIndex();

        if (parent_frame != child_frame)
            child_context->SetFrameIndex(parent_frame);
    }

    void TickSubWorldComponents(hgl::ecs::ECSContext* context, float delta_time)
    {
        const auto stats = DispatchLocalLogicSubWorldComponents(
            context,
            [delta_time](hgl::ecs::SubWorldComponent& sub_world)
            {
                sub_world.UpdateSubWorld(delta_time);
            });

#if ULRE_ECS_DEBUG_API
        static bool logged_once = false;
        if (!logged_once && stats.shared_count > 0)
        {
            logged_once = true;
            GLogDebug("[World] TickSubWorldComponents: shared=%u isolated=%u dispatched=%u",
                     stats.shared_count,
                     stats.isolated_count,
                     stats.dispatched_count);
        }
#endif
    }

    void RenderSubWorldComponents(hgl::ecs::ECSContext* context, hgl::graph::RenderCmdBuffer* cmd, float delta_time)
    {
        const auto stats = DispatchLocalRenderSubWorldComponents(
            context,
            [cmd, delta_time](hgl::ecs::SubWorldComponent& sub_world)
            {
                sub_world.RenderSubWorld(cmd, delta_time);
            });

#if ULRE_ECS_DEBUG_API
        static bool logged_once = false;
        if (!logged_once && stats.shared_count > 0)
        {
            logged_once = true;
            GLogDebug("[World] RenderSubWorldComponents: shared=%u isolated=%u dispatched=%u",
                     stats.shared_count,
                     stats.isolated_count,
                     stats.dispatched_count);
        }
#endif
    }

    void SyncSharedRenderBridgeSubWorldComponents(hgl::ecs::ECSContext* context, float delta_time)
    {
        const auto stats = DispatchHybridBridgeSubWorldComponents(
            context,
            [delta_time](hgl::ecs::SubWorldComponent& sub_world)
            {
                sub_world.SyncSharedRenderBridge(delta_time);
            });

#if ULRE_ECS_DEBUG_API
        static bool logged_once = false;
        if (!logged_once && stats.dispatched_count > 0)
        {
            logged_once = true;
            GLogDebug("[World] SyncSharedRenderBridgeSubWorldComponents: non_hybrid=%u hybrid=%u dispatched=%u",
                     stats.shared_count,
                     stats.isolated_count,
                     stats.dispatched_count);
        }
#endif
    }
}

namespace hgl::ecs
{
    World::World(const std::string& name)
        : Object(name)
        , context(std::make_shared<ECSContext>(name + "_Context"))
    {
    }

    World::World(std::shared_ptr<ECSContext> ctx, const std::string& name)
        : Object(name)
        , context(std::move(ctx))
    {
        if (!context)
            context = std::make_shared<ECSContext>(name + "_Context");
    }

    void World::Initialize()
    {
        if (context)
        {
            context->SetSubWorldAutoUpdate(false);
            context->Initialize();
        }
    }

    void World::Shutdown()
    {
        for (auto& child : children)
        {
            if (child)
                child->Shutdown();
        }

        if (context)
            context->Shutdown();
    }

    void World::Tick(float delta_time)
    {
        if (!active)
            return;

        if (is_ticking)
        {
            LogWarning("[World] Tick re-entry blocked: %s", GetName().c_str());
            return;
        }

        is_ticking = true;

        if (context)
        {
            context->SetSubWorldAutoUpdate(false);
            context->Tick(delta_time);
            TickSubWorldComponents(context.get(), delta_time);
        }

        for (auto& child : children)
        {
            if (child)
            {
                SyncChildFrameIndex(context.get(), child->GetContext());
                child->Tick(delta_time);
            }
        }

        is_ticking = false;
    }

    void World::Render(graph::RenderCmdBuffer* cmd, float delta_time)
    {
        if (!active)
            return;

        if (is_rendering)
        {
            LogWarning("[World] Render re-entry blocked: %s", GetName().c_str());
            return;
        }

        is_rendering = true;

        if (context)
        {
            context->SetSubWorldAutoUpdate(false);

            // H3 sync point: bridge hybrid local-logic state into shared render input
            // before root render systems begin collect/batch.
            SyncSharedRenderBridgeSubWorldComponents(context.get(), delta_time);

            context->Render(cmd, delta_time);
            RenderSubWorldComponents(context.get(), cmd, delta_time);
        }

        for (auto& child : children)
        {
            if (child)
            {
                SyncChildFrameIndex(context.get(), child->GetContext());
                child->Render(cmd, delta_time);
            }
        }

        is_rendering = false;
    }

    void World::AddChild(const std::shared_ptr<World>& child)
    {
        if (!child || child.get() == this)
            return;

        for (const auto& entry : children)
        {
            if (entry.get() == child.get())
                return;
        }

        children.push_back(child);
    }

    bool World::RemoveChild(World* child)
    {
        if (!child)
            return false;

        const auto old_size = children.size();
        children.erase(std::remove_if(children.begin(), children.end(),
                        [child](const std::shared_ptr<World>& entry)
                        {
                            return !entry || entry.get() == child;
                        }),
                        children.end());

        return children.size() != old_size;
    }

    void World::ClearChildren()
    {
        children.clear();
    }
}
