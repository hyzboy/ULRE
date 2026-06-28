#pragma once

#include <hgl/ecs/core/System.h>
#include <string>

namespace hgl::ecs
{
    class RenderDiagnosticsService : public System
    {
    private:

        class ECSContext *world = nullptr;
        uint64_t last_emit_ms = 0;

    public:

        RenderDiagnosticsService(const std::string &name = "RenderDiagnosticsService");
        ~RenderDiagnosticsService() override = default;

    public:

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;
    };
}
