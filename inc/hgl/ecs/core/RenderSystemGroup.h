#pragma once

#include <hgl/ecs/core/System.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace hgl
{
    namespace ecs
    {
        /**
         * RenderSystemGroup - A named group of render systems that execute in a phase range
         * Allows dynamic enable/disable of entire system groups based on scene content
         */
        struct RenderSystemGroup
        {
            /// Unique name identifying this group (e.g., "Primitive", "Text", "Line", "Billboard")
            std::string name;

            /// Starting ExecutionPhase for this group
            ExecutionPhase startPhase;

            /// Ending ExecutionPhase for this group (inclusive)
            ExecutionPhase endPhase;

            /// Whether this group is currently enabled
            bool enabled = true;

            RenderSystemGroup() = default;

            RenderSystemGroup(const std::string& n, ExecutionPhase start, ExecutionPhase end, bool en = true)
                : name(n), startPhase(start), endPhase(end), enabled(en) {}
        };

        /**
         * Global registry for render system groups
         * Each render element type (Primitive, Text, Line, Billboard, etc.) registers its phase range
         * Enables dynamic RenderGraph construction based on scene content and registered groups
         */
        class RenderSystemGroupRegistry
        {
        private:
            /// Static singleton instance
            static RenderSystemGroupRegistry* instance;

            /// Map from group name to group definition
            std::map<std::string, RenderSystemGroup> groups;

            RenderSystemGroupRegistry() = default;

        public:
            /// Get singleton instance
            static RenderSystemGroupRegistry& Get();

            /// Register a new render system group
            /// If a group with the same name exists, it will be overwritten
            void Register(const RenderSystemGroup& group);

            /// Enable/disable a system group by name
            /// Returns false if group not found
            bool SetGroupEnabled(const std::string& name, bool enabled);

            /// Check if a group is enabled
            /// Returns false if group not found
            bool IsGroupEnabled(const std::string& name) const;

            /// Get all registered groups
            std::vector<RenderSystemGroup> GetAllGroups() const;

            /// Get only enabled groups (sorted by startPhase)
            std::vector<RenderSystemGroup> GetEnabledGroups() const;

            /// Get a specific group by name (returns nullptr if not found)
            const RenderSystemGroup* GetGroup(const std::string& name) const;

            /// Get group by name (mutable)
            RenderSystemGroup* GetGroupMutable(const std::string& name);

            /// Clear all registered groups
            void Clear();

            /// Print group registry info for debugging
            void DebugPrint() const;
        };

        /**
         * Helper RAII class for temporarily enabling/disabling a group
         * Restores previous state on destruction
         */
        class ScopedGroupState
        {
        private:
            std::string group_name;
            bool prev_enabled;

        public:
            ScopedGroupState(const std::string& name, bool enable_state)
                : group_name(name), prev_enabled(false)
            {
                RenderSystemGroupRegistry::Get().SetGroupEnabled(name, enable_state);
            }

            ~ScopedGroupState()
            {
                RenderSystemGroupRegistry::Get().SetGroupEnabled(group_name, prev_enabled);
            }

            // Non-copyable
            ScopedGroupState(const ScopedGroupState&) = delete;
            ScopedGroupState& operator=(const ScopedGroupState&) = delete;
        };

    } // namespace ecs
} // namespace hgl
