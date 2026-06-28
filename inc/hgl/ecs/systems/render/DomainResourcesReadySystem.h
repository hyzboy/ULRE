#pragma once

#include <hgl/ecs/core/System.h>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace hgl::ecs
{
    class DomainResourcesReadySystem : public System
    {
    public:

        struct DomainReadinessState
        {
            bool exists = false;
            bool dirty = false;
            bool has_texture_array = false;
            bool has_sampler = false;
            bool has_material = false;
            bool has_dmb = false;
            bool has_primitive = false;
            bool ready = false;
        };

    private:

        class ECSContext *world = nullptr;
        std::unordered_map<std::string, DomainReadinessState> domain_states;

    public:

        DomainResourcesReadySystem(const std::string &name = "DomainResourcesReadySystem");
        ~DomainResourcesReadySystem() override = default;

    public:

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;

        bool IsDomainReady(const std::string &domain_tag) const;
        const DomainReadinessState *GetDomainState(const std::string &domain_tag) const;
    };
}
