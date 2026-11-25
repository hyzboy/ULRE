#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/System.h>
#include <algorithm>

namespace hgl::ecs
{
    void ECSContext::GetSystemsByElementType(const std::string& element_type, std::vector<std::shared_ptr<System>>& out_systems) const
    {
        out_systems.clear();
        if (const auto* systems_list = system_registry.FindByElementType(AnsiString(element_type.c_str())))
        {
            out_systems.reserve(systems_list->GetCount());
            for (const auto& system : *systems_list)
                out_systems.push_back(system);
        }
    }

    void ECSContext::GetAllRenderElementTypes(std::vector<std::string>& out_element_types) const
    {
        out_element_types.clear();
        system_registry.ForEachElementType([&out_element_types](const AnsiString& name)
        {
            out_element_types.emplace_back(name.c_str());
        });

        // 注册表内部无序存储；唯一有序消费者在此：按名排序保持旧 std::map 的遍历顺序
        // （RenderGraph::EnsureSystemGroupsRegistered 依此顺序注册系统组）。
        std::sort(out_element_types.begin(), out_element_types.end());
    }

    void ECSContext::SetSystemGroupEnabled(SystemGroupID group, bool enabled)
    {
        system_registry.SetGroupEnabled(group, enabled);
    }

    bool ECSContext::IsSystemGroupInstalled(SystemGroupID group) const
    {
        if (group == SystemGroupID::None)
            return false;

        return system_group_activity.IsInstalled(group);
    }

    void ECSContext::MarkSystemGroupInstalled(SystemGroupID group)
    {
        if (group == SystemGroupID::None)
            return;

        system_group_activity.MarkInstalled(group);
    }

    void ECSContext::ResetComponentDrivenGroups()
    {
        system_group_activity.Reset();
        system_registry.ClearGroupSnapshots();
    }
}
