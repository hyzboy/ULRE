#include <hgl/ecs/systems/render/DomainResourcesReadySystem.h>
#include <hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include <hgl/graph/module/TextureDomainRegistry.h>
#include <hgl/log/Log.h>
#include <hgl/time/Time.h>

namespace hgl::ecs
{
    namespace
    {
        static uint64_t g_domain_ready_last_log_ms = 0;
    }

    DomainResourcesReadySystem::DomainResourcesReadySystem(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Primitive");
        AddDependency<QuadResourcePrepareSystem>();
    }

    void DomainResourcesReadySystem::Update(float)
    {
        domain_states.clear();

        uint32_t total_count = 0;
        uint32_t ready_count = 0;
        uint32_t not_ready_count = 0;
        uint32_t dirty_count = 0;

        graph::TextureDomainRegistry::ForEach(
            [this, &total_count, &ready_count, &not_ready_count, &dirty_count]
            (const std::string &domain_tag, graph::TextureDomainRegistry::DomainEntry &entry)
            {
                DomainReadinessState state{};
                state.exists = true;
                state.dirty = entry.dirty;
                state.has_texture_array = (entry.texture_array != nullptr);
                state.has_sampler = (entry.sampler != nullptr);
                state.has_material = (entry.material != nullptr);
                state.has_dmb = (entry.dmb != nullptr);
                state.has_primitive = (entry.primitive != nullptr);

                state.ready = !state.dirty
                           && state.has_texture_array
                           && state.has_sampler
                           && state.has_material
                           && state.has_dmb
                           && state.has_primitive;

                ++total_count;
                if (state.dirty)
                    ++dirty_count;

                if (state.ready)
                    ++ready_count;
                else
                    ++not_ready_count;

                domain_states[domain_tag] = state;
            });

        const uint64_t now_ms = ::hgl::GetTimeMs();
        if (g_domain_ready_last_log_ms == 0)
        {
            g_domain_ready_last_log_ms = now_ms;
            return;
        }

        if (now_ms - g_domain_ready_last_log_ms < 1000)
            return;

        GLogInfo("[PhaseD][DomainResourcesReadySystem] domains=%u ready=%u not_ready=%u dirty=%u",
                total_count,
                ready_count,
                not_ready_count,
                dirty_count);

        g_domain_ready_last_log_ms = now_ms;
    }

    bool DomainResourcesReadySystem::IsDomainReady(const std::string &domain_tag) const
    {
        const auto *state = GetDomainState(domain_tag);
        return state && state->ready;
    }

    const DomainResourcesReadySystem::DomainReadinessState *
    DomainResourcesReadySystem::GetDomainState(const std::string &domain_tag) const
    {
        auto it = domain_states.find(domain_tag);
        if (it == domain_states.end())
            return nullptr;

        return &it->second;
    }
}
