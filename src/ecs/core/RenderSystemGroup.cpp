#include <hgl/ecs/core/RenderSystemGroup.h>
#include <hgl/log/Log.h>
#include <algorithm>

namespace hgl
{
    namespace ecs
    {
        // Static instance initialization
        RenderSystemGroupRegistry* RenderSystemGroupRegistry::instance = nullptr;

        RenderSystemGroupRegistry& RenderSystemGroupRegistry::Get()
        {
            if (!instance)
                instance = new RenderSystemGroupRegistry();
            return *instance;
        }

        void RenderSystemGroupRegistry::Register(const RenderSystemGroup& group)
        {
            if (group.name.empty())
            {
                GLogWarning("[RenderSystemGroup] Attempting to register group with empty name");
                return;
            }

            auto it = groups.find(group.name);
            if (it != groups.end())
            {
                GLogDebug("[RenderSystemGroup] Overwriting existing group: %s (phases %d-%d) -> (%d-%d)",
                         group.name.c_str(),
                         static_cast<int>(it->second.startPhase),
                         static_cast<int>(it->second.endPhase),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase));
            }
            else
            {
                GLogDebug("[RenderSystemGroup] Registering new group: %s (phases %d-%d)",
                         group.name.c_str(),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase));
            }

            groups[group.name] = group;
        }

        bool RenderSystemGroupRegistry::SetGroupEnabled(const std::string& name, bool enabled)
        {
            auto it = groups.find(name);
            if (it == groups.end())
            {
                GLogWarning("[RenderSystemGroup] Attempted to set enabled state for non-existent group: %s", name.c_str());
                return false;
            }

            if (it->second.enabled != enabled)
            {
                GLogDebug("[RenderSystemGroup] Group '%s' %s", name.c_str(), enabled ? "ENABLED" : "DISABLED");
                it->second.enabled = enabled;
            }

            return true;
        }

        bool RenderSystemGroupRegistry::IsGroupEnabled(const std::string& name) const
        {
            auto it = groups.find(name);
            if (it == groups.end())
                return false;
            return it->second.enabled;
        }

        std::vector<RenderSystemGroup> RenderSystemGroupRegistry::GetAllGroups() const
        {
            std::vector<RenderSystemGroup> result;
            for (const auto& [name, group] : groups)
            {
                result.push_back(group);
            }
            // Sort by startPhase for consistent ordering
            std::sort(result.begin(), result.end(),
                     [](const RenderSystemGroup& a, const RenderSystemGroup& b)
                     { return a.startPhase < b.startPhase; });
            return result;
        }

        std::vector<RenderSystemGroup> RenderSystemGroupRegistry::GetEnabledGroups() const
        {
            std::vector<RenderSystemGroup> result;
            for (const auto& [name, group] : groups)
            {
                if (group.enabled)
                    result.push_back(group);
            }
            // Sort by startPhase for consistent ordering
            std::sort(result.begin(), result.end(),
                     [](const RenderSystemGroup& a, const RenderSystemGroup& b)
                     { return a.startPhase < b.startPhase; });
            return result;
        }

        const RenderSystemGroup* RenderSystemGroupRegistry::GetGroup(const std::string& name) const
        {
            auto it = groups.find(name);
            if (it == groups.end())
                return nullptr;
            return &it->second;
        }

        RenderSystemGroup* RenderSystemGroupRegistry::GetGroupMutable(const std::string& name)
        {
            auto it = groups.find(name);
            if (it == groups.end())
                return nullptr;
            return &it->second;
        }

        void RenderSystemGroupRegistry::Clear()
        {
            GLogDebug("[RenderSystemGroup] Clearing all %zu registered groups", groups.size());
            groups.clear();
        }

        void RenderSystemGroupRegistry::DebugPrint() const
        {
            GLogInfo("[RenderSystemGroup] === RenderSystemGroup Registry ===");
            GLogInfo("[RenderSystemGroup] Total groups: %zu", groups.size());

            for (const auto& [name, group] : groups)
            {
                GLogInfo("[RenderSystemGroup]   %-12s [%3d - %3d] %s",
                         name.c_str(),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase),
                         group.enabled ? "ENABLED" : "DISABLED");
            }

            GLogInfo("[RenderSystemGroup] ===================================");
        }

    } // namespace ecs
} // namespace hgl
