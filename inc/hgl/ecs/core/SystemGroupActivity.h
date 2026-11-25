#pragma once

#include <hgl/ecs/support/storage/ComponentTypeTable.h>

namespace hgl::ecs
{
    class SystemGroupActivity
    {
        uint32_t component_counts[kSystemGroupCount] = {};
        bool installed[kSystemGroupCount] = {};

    public:
        void Add(SystemGroupID group)
        {
            ++component_counts[static_cast<uint32_t>(group)];
        }

        bool Remove(SystemGroupID group)
        {
            uint32_t &count = component_counts[static_cast<uint32_t>(group)];
            if (count > 0)
                --count;
            return count == 0;
        }

        bool IsInstalled(SystemGroupID group) const
        {
            return installed[static_cast<uint32_t>(group)];
        }

        void MarkInstalled(SystemGroupID group)
        {
            installed[static_cast<uint32_t>(group)] = true;
        }

        void Reset()
        {
            for (uint32_t group = 0; group < kSystemGroupCount; ++group)
            {
                component_counts[group] = 0;
                installed[group] = false;
            }
        }
    };
}
