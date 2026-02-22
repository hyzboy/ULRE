#pragma once

#include<string>
#include <hgl/type/UnorderedMap.h>
#include<chrono>
#include<cstdint>
#include<map>

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

        struct SystemGroupProfileData
        {
            std::string groupName;
            uint32_t componentCount = 0;
            bool enabled = false;
            uint64_t ensureCount = 0;
            uint64_t activationCount = 0;
            uint64_t deactivationCount = 0;
        };

    public:
        void Begin(System* system);
        void End(System* system);
        void Reset();

        void MarkGroupEnsured(const std::string& group_name);
        void UpdateGroupState(const std::string& group_name, uint32_t component_count, bool enabled);
        void RemoveGroupProfile(const std::string& group_name);

        const hgl::UnorderedMap<System*, ProfileData>& GetProfiles() const { return profiles; }
        const std::map<std::string, SystemGroupProfileData>& GetSystemGroupProfiles() const { return group_profiles; }

    private:
        hgl::UnorderedMap<System*, ProfileData> profiles;
        std::map<std::string, SystemGroupProfileData> group_profiles;
    };
}//namespace hgl::ecs

