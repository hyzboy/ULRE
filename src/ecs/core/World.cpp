#include<hgl/ecs/core/World.h>

namespace hgl::ecs
{
    World::World(const std::string& name)
        : Object(name)
        , context(std::make_shared<ECSContext>(name + "_Context"))
        , scheduler(std::make_unique<WorldScheduler>())
    {
    }

    World::World(std::shared_ptr<ECSContext> ctx, const std::string& name)
        : Object(name)
        , context(std::move(ctx))
        , scheduler(std::make_unique<WorldScheduler>())
    {
        if (!context)
            context = std::make_shared<ECSContext>(name + "_Context");
    }

    void World::SetContext(const std::shared_ptr<ECSContext>& ctx)
    {
        context = ctx;

        if (!scheduler)
            scheduler = std::make_unique<WorldScheduler>();

        scheduler->MarkTopologyDirty();
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

        if (!scheduler)
            scheduler = std::make_unique<WorldScheduler>();

        scheduler->Rebuild(this);
        scheduler->Tick(delta_time);

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

        if (!scheduler)
            scheduler = std::make_unique<WorldScheduler>();

        scheduler->Rebuild(this);
        scheduler->Render(cmd, delta_time);

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

        if (scheduler)
            scheduler->MarkTopologyDirty();
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

        const bool changed = children.size() != old_size;
        if (changed && scheduler)
            scheduler->MarkTopologyDirty();

        return changed;
    }

    void World::ClearChildren()
    {
        children.clear();

        if (scheduler)
            scheduler->MarkTopologyDirty();
    }
}
