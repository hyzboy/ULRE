#include<hgl/ecs/core/SystemProfiler.h>
#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    void SystemProfiler::Begin(System* system)
    {
        if (!system)
            return;

        auto *data = profiles.GetValuePointer(system);
        if (!data)
        {
            profiles.Add(system, ProfileData{});
            data = profiles.GetValuePointer(system);
        }

        if (!data)
            return;

        data->systemName = system->GetName();
        data->startTime = std::chrono::high_resolution_clock::now();
        data->running = true;
    }

    void SystemProfiler::End(System* system)
    {
        if (!system)
            return;

        auto *data = profiles.GetValuePointer(system);
        if (!data)
            return;

        if (!data->running)
            return;

        const auto endTime = std::chrono::high_resolution_clock::now();
        const float duration = std::chrono::duration<float, std::milli>(endTime - data->startTime).count();

        data->lastUpdateMs = duration;
        data->maxUpdateMs = (data->updateCount == 0) ? duration : std::max(data->maxUpdateMs, duration);

        const float alpha = 0.1f;
        if (data->updateCount == 0)
            data->averageUpdateMs = duration;
        else
            data->averageUpdateMs = alpha * duration + (1.0f - alpha) * data->averageUpdateMs;

        ++data->updateCount;
        data->running = false;
    }

    void SystemProfiler::Reset()
    {
        profiles.Clear();
        group_profiles.clear();
    }

    void SystemProfiler::MarkGroupEnsured(const std::string& group_name)
    {
        if (group_name.empty())
            return;

        auto& data = group_profiles[group_name];
        data.groupName = group_name;
        ++data.ensureCount;
    }

    void SystemProfiler::UpdateGroupState(const std::string& group_name, uint32_t component_count, bool enabled)
    {
        if (group_name.empty())
            return;

        auto& data = group_profiles[group_name];
        data.groupName = group_name;
        data.componentCount = component_count;

        if (data.enabled != enabled)
        {
            if (enabled)
                ++data.activationCount;
            else
                ++data.deactivationCount;
        }

        data.enabled = enabled;
    }

    void SystemProfiler::RemoveGroupProfile(const std::string& group_name)
    {
        if (group_name.empty())
            return;

        group_profiles.erase(group_name);
    }
}//namespace hgl::ecs

