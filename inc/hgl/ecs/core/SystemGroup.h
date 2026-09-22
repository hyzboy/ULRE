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

            SystemGroup() = default;

            SystemGroup(const std::string& n, ExecutionPhase start, ExecutionPhase end)
                : name(n), startPhase(start), endPhase(end) {}
        };

        /**
         * Global registry for system groups (group definitions + plugin installers)
         *
         * 只承载真正的全局不变量：组定义（name + 相位区间）与安装器。
         * 组的"是否启用"是每世界状态，由 CreateAdaptiveRenderGraph 按
         * 本世界 SceneStats 即时推导——不再存入本注册表（历史上存全局
         * 会在多世界建图时互相覆盖）。
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

            /// Get all registered groups
            std::vector<SystemGroup> GetAllGroups() const;

            /// Get a specific group by name (returns nullptr if not found)
            const SystemGroup* GetGroup(const std::string& name) const;

            /// Print group registry info for debugging
            void DebugPrint() const;
        };

    } // namespace ecs
} // namespace hgl
