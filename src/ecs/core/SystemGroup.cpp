#include <hgl/ecs/core/SystemGroup.h>
#include <hgl/log/Log.h>
#include <algorithm>

namespace hgl
{
    namespace ecs
    {
        // Static instance initialization
        SystemGroupRegistry* SystemGroupRegistry::instance = nullptr;

        SystemGroupRegistry& SystemGroupRegistry::Get()
        {
            if (!instance)
                instance = new SystemGroupRegistry();
            return *instance;
        }

        void SystemGroupRegistry::RegisterGroup(const SystemGroup& group)
        {
            if (group.name.empty())
            {
                GLogWarning("[SystemGroup] Attempting to register group with empty name");
                return;
            }

            auto it = groups.find(group.name);
            if (it != groups.end())
            {
                GLogDebug("[SystemGroup] Overwriting existing group: %s (phases %d-%d) -> (%d-%d)",
                         group.name.c_str(),
                         static_cast<int>(it->second.startPhase),
                         static_cast<int>(it->second.endPhase),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase));
            }
            else
            {
                GLogDebug("[SystemGroup] Registering new group: %s (phases %d-%d)",
                         group.name.c_str(),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase));
            }

            groups[group.name] = group;
        }

        void SystemGroupRegistry::RegisterGroupInstaller(const std::string& name, GroupInstaller installer)
        {
            if (name.empty() || !installer)
                return;

            installers[name] = std::move(installer);
        }

        bool SystemGroupRegistry::HasGroupInstaller(const std::string& name) const
        {
            return installers.find(name) != installers.end();
        }

        bool SystemGroupRegistry::EnsureGroupSystems(const std::string& name, ECSContext* context, hgl::graph::IRenderTarget* default_rt)
        {
            auto it = installers.find(name);
            if (it == installers.end())
                return false;

            return it->second(context, default_rt);
        }

        bool SystemGroupRegistry::SetGroupEnabled(const std::string& name, bool enabled)
        {
            auto it = groups.find(name);
            if (it == groups.end())
            {
                GLogWarning("[SystemGroup] Attempted to set enabled state for non-existent group: %s", name.c_str());
                return false;
            }

            if (it->second.enabled != enabled)
            {
                GLogDebug("[SystemGroup] Group '%s' %s", name.c_str(), enabled ? "ENABLED" : "DISABLED");
                it->second.enabled = enabled;
            }

            return true;
        }

        bool SystemGroupRegistry::IsGroupEnabled(const std::string& name) const
        {
            auto it = groups.find(name);
            if (it == groups.end())
                return false;
            return it->second.enabled;
        }

        std::vector<SystemGroup> SystemGroupRegistry::GetAllGroups() const
        {
            std::vector<SystemGroup> result;
            for (const auto& [name, group] : groups)
            {
                result.push_back(group);
            }
            // Sort by startPhase for consistent ordering
            std::sort(result.begin(), result.end(),
                     [](const SystemGroup& a, const SystemGroup& b)
                     { return a.startPhase < b.startPhase; });
            return result;
        }

        std::vector<SystemGroup> SystemGroupRegistry::GetEnabledGroups() const
        {
            std::vector<SystemGroup> result;
            for (const auto& [name, group] : groups)
            {
                if (group.enabled)
                    result.push_back(group);
            }
            // Sort by startPhase for consistent ordering
            std::sort(result.begin(), result.end(),
                     [](const SystemGroup& a, const SystemGroup& b)
                     { return a.startPhase < b.startPhase; });
            return result;
        }

        const SystemGroup* SystemGroupRegistry::GetGroup(const std::string& name) const
        {
            auto it = groups.find(name);
            if (it == groups.end())
                return nullptr;
            return &it->second;
        }

        SystemGroup* SystemGroupRegistry::GetGroupMutable(const std::string& name)
        {
            auto it = groups.find(name);
            if (it == groups.end())
                return nullptr;
            return &it->second;
        }

        void SystemGroupRegistry::Clear()
        {
            GLogDebug("[SystemGroup] Clearing all %zu registered groups", groups.size());
            groups.clear();
        }

        void SystemGroupRegistry::DebugPrint() const
        {
            GLogInfo("[SystemGroup] === SystemGroup Registry ===");
            GLogInfo("[SystemGroup] Total groups: %zu", groups.size());

            for (const auto& [name, group] : groups)
            {
                GLogInfo("[SystemGroup]   %-12s [%3d - %3d] %s",
                         name.c_str(),
                         static_cast<int>(group.startPhase),
                         static_cast<int>(group.endPhase),
                         group.enabled ? "ENABLED" : "DISABLED");
            }

            GLogInfo("[SystemGroup] ===================================");
        }

    } // namespace ecs
} // namespace hgl
