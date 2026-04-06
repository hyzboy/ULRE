#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;

    class LineStatsSystem : public System
    {
    private:
        ECSContext* world = nullptr;
        uint32_t frame_counter = 0;
        uint32_t log_interval = 120;

    public:
        explicit LineStatsSystem(const std::string& name = "LineStatsSystem");
        ~LineStatsSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void SetLogInterval(uint32_t interval) { log_interval = interval > 0 ? interval : 1; }

        void Update(float deltaTime) override;
    };
}
