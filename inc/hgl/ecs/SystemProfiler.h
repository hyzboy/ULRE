#pragma once

#include<string>
#include <hgl/type/UnorderedMap.h>
#include<chrono>
#include<cstdint>

namespace hgl::ecs
{
    class System;

    /**
     * SystemProfiler
     *
     * Tracks per-system update timings for performance diagnostics.
     */
    class SystemProfiler
    {
    public:
        struct ProfileData
        {
            std::string systemName;
            float lastUpdateMs = 0.0f;
            float averageUpdateMs = 0.0f;
            float maxUpdateMs = 0.0f;
            uint64_t updateCount = 0;
            std::chrono::high_resolution_clock::time_point startTime;
            bool running = false;
        };

    public:
        void Begin(System* system);
        void End(System* system);
        void Reset();

        const hgl::UnorderedMap<System*, ProfileData>& GetProfiles() const { return profiles; }

    private:
        hgl::UnorderedMap<System*, ProfileData> profiles;
    };
}//namespace hgl::ecs

