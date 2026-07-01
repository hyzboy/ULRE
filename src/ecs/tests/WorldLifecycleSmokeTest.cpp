#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/World.h>

#include <cstdio>
#include <memory>

namespace hgl::ecs::tests
{
    class CounterTickSystem final : public System
    {
    public:

        explicit CounterTickSystem(int* counter)
            : System("CounterTickSystem")
            , tick_counter(counter)
        {
            SetExecutionOrder(ExecutionPhase::TickInput);
        }

        void Update(float) override
        {
            if (tick_counter)
                ++(*tick_counter);
        }

    private:

        int* tick_counter = nullptr;
    };

    class ReentrantTickSystem final : public System
    {
    public:

        ReentrantTickSystem(World* owner_world, int* counter)
            : System("ReentrantTickSystem")
            , world(owner_world)
            , tick_counter(counter)
        {
            SetExecutionOrder(ExecutionPhase::TickInput);
        }

        void Update(float) override
        {
            if (tick_counter)
                ++(*tick_counter);

            if (!attempted_reentry && world)
            {
                attempted_reentry = true;
                world->Tick(0.0f);
            }
        }

    private:

        World* world = nullptr;
        int* tick_counter = nullptr;
        bool attempted_reentry = false;
    };

    class CounterRenderSystem final : public System
    {
    public:

        explicit CounterRenderSystem(int* counter)
            : System("CounterRenderSystem")
            , render_counter(counter)
        {
            SetExecutionOrder(ExecutionPhase::RenderDrawSubmit);
        }

        void Render(graph::RenderCmdBuffer*, float) override
        {
            if (render_counter)
                ++(*render_counter);
        }

    private:

        int* render_counter = nullptr;
    };
}

namespace
{
    int ReportFailure(const char* message)
    {
        std::fprintf(stderr, "[ECSWorldLifecycleSmokeTest] FAILED: %s\n", message);
        return 1;
    }
}

int main()
{
    using namespace hgl::ecs;
    using namespace hgl::ecs::tests;

    {
        auto root_world = std::make_shared<World>("RootWorld");
        auto child_world = std::make_shared<World>("ChildWorld");
        root_world->AddChild(child_world);

        int root_tick_count = 0;
        int reentrant_tick_count = 0;
        int child_tick_count = 0;

        root_world->RegisterTickSystem<CounterTickSystem>(&root_tick_count);
        root_world->RegisterTickSystem<ReentrantTickSystem>(root_world.get(), &reentrant_tick_count);
        child_world->RegisterTickSystem<CounterTickSystem>(&child_tick_count);

        root_world->Initialize();
        child_world->Initialize();

        root_world->Tick(0.016f);

        if (root_tick_count != 1)
            return ReportFailure("Root tick system should run exactly once per Tick call.");

        if (reentrant_tick_count != 1)
            return ReportFailure("Reentrant tick should be blocked by World::Tick re-entry guard.");

        if (child_tick_count != 1)
            return ReportFailure("Child world should tick exactly once when parent ticks.");

        root_world->SetActive(false);
        root_world->Tick(0.016f);

        if (root_tick_count != 1 || reentrant_tick_count != 1 || child_tick_count != 1)
            return ReportFailure("Inactive parent world should block Tick for itself and children.");

        root_world->Shutdown();
        child_world->Shutdown();
    }

    {
        auto root_world = std::make_shared<World>("RootRenderWorld");
        auto child_world = std::make_shared<World>("ChildRenderWorld");
        root_world->AddChild(child_world);

        int root_render_count = 0;
        int child_render_count = 0;

        root_world->RegisterRenderSystem<CounterRenderSystem>(&root_render_count);
        child_world->RegisterRenderSystem<CounterRenderSystem>(&child_render_count);

        root_world->Initialize();
        child_world->Initialize();

        root_world->Render(nullptr, 0.016f);

        if (root_render_count != 1)
            return ReportFailure("Root render system should run exactly once per Render call.");

        if (child_render_count != 1)
            return ReportFailure("Child world should render exactly once when parent renders.");

        root_world->SetActive(false);
        root_world->Render(nullptr, 0.016f);

        if (root_render_count != 1 || child_render_count != 1)
            return ReportFailure("Inactive parent world should block Render for itself and children.");

        root_world->Shutdown();
        child_world->Shutdown();
    }

    std::printf("[ECSWorldLifecycleSmokeTest] PASSED\n");
    return 0;
}
