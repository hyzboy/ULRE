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
    }
}//namespace hgl::ecs

