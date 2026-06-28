#include <hgl/ecs/systems/render/DomainResourcesReadySystem.h>
#include <hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include <hgl/graph/module/TextureDomainRegistry.h>

namespace hgl::ecs
{
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

        graph::TextureDomainRegistry::ForEach(
            [this](const std::string &domain_tag, graph::TextureDomainRegistry::DomainEntry &entry)
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

                domain_states[domain_tag] = state;
            });
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
