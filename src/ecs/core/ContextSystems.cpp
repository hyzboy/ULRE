#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/system/System.h>
namespace hgl::ecs
{
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
