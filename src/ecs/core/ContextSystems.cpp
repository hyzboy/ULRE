#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/system/System.h>
namespace hgl::ecs
{
    void ECSContext::SetSystemGroupEnabled(SystemGroupID group, bool enabled)
    {
        system_registry.SetGroupEnabled(group, enabled);
    }

}
