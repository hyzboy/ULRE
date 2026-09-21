#pragma once

#include <hgl/ecs/core/System.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
    }
}

namespace hgl
{
    namespace ecs
    {
        class ECSContext;

        /**
         * SystemGroup - A named group of systems that execute in a phase range
         * Allows dynamic enable/disable of entire system groups based on scene/runtime content
         */
        struct SystemGroup
        {
            /// Unique name identifying this group (e.g., "Primitive", "Text", "Line")
            std::string name;

            /// Starting ExecutionPhase for this group
            ExecutionPhase startPhase;

            /// Ending ExecutionPhase for this group (inclusive)
            ExecutionPhase endPhase;

            /// Whether this group is currently enabled
            bool enabled = true;

            SystemGroup() = default;

            SystemGroup(const std::string& n, ExecutionPhase start, ExecutionPhase end, bool en = true)
                : name(n), startPhase(start), endPhase(end), enabled(en) {}
        };

        /**
         * Global registry for system groups
         * Supports both metadata (phase range, enabled state) and plugin installers.
         */
        class SystemGroupRegistry
        {
        private:
            /// Static singleton instance
            static SystemGroupRegistry* instance;

            /// Map from group name to group definition
            std::map<std::string, SystemGroup> groups;

            using GroupInstaller = std::function<bool(ECSContext*, hgl::graph::IRenderTarget*)>;
            std::map<std::string, GroupInstaller> installers;

            SystemGroupRegistry() = default;

        public:
            /// Get singleton instance
            static SystemGroupRegistry& Get();

            /// Register a new system group
            /// If a group with the same name exists, it will be overwritten
            void RegisterGroup(const SystemGroup& group);

            /// Register installer callback for a group (plugin style)
            void RegisterGroupInstaller(const std::string& name, GroupInstaller installer);

            /// Ensure systems for a group are installed
            bool EnsureGroupSystems(const std::string& name, ECSContext* context, hgl::graph::IRenderTarget* default_rt);

            /// Enable/disable a system group by name
            /// Returns false if group not found
            bool SetGroupEnabled(const std::string& name, bool enabled);

            /// Get all registered groups
            std::vector<SystemGroup> GetAllGroups() const;

            /// Get only enabled groups (sorted by startPhase)
            std::vector<SystemGroup> GetEnabledGroups() const;

            /// Get a specific group by name (returns nullptr if not found)
            const SystemGroup* GetGroup(const std::string& name) const;

            /// Print group registry info for debugging
            void DebugPrint() const;
        };

    } // namespace ecs
} // namespace hgl
