#include<hgl/ecs/core/World.h>
#include<hgl/ecs/components/SubWorldComponent.h>

namespace
{
    void SyncChildFrameIndex(hgl::ecs::ECSContext* parent_context, hgl::ecs::ECSContext* child_context)
    {
        if (!parent_context || !child_context)
            return;

        const uint32_t parent_frame = parent_context->GetFrameIndex();
        const uint32_t child_frame = child_context->GetFrameIndex();

        if (parent_frame != child_frame)
        {
            child_context->SetFrameIndex(parent_frame);
        }
    }

    void TickSubWorldComponents(hgl::ecs::ECSContext* context, float delta_time)
    {
        if (!context)
            return;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (sub_world)
                sub_world->UpdateSubWorld(delta_time);
        }
    }

    void RenderSubWorldComponents(hgl::ecs::ECSContext* context, hgl::graph::RenderCmdBuffer* cmd, float delta_time)
    {
        if (!context)
            return;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (sub_world)
                sub_world->RenderSubWorld(cmd, delta_time);
        }
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
