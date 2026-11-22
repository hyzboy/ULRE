#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/ecs/support/debug/DebugViewPass.h>

namespace hgl::ecs
{
    bool DebugViewController::RequestMode(graph::mtl::DebugViewMode mode)
    {
        return DebugViewPass::RequestMode(mode);
    }

    graph::mtl::DebugViewMode DebugViewController::GetRequestedMode()
    {
        return DebugViewPass::GetRequestedMode();
    }

    graph::mtl::DebugViewMode DebugViewController::GetAppliedMode()
    {
        return DebugViewPass::GetAppliedMode();
    }

    uint32 DebugViewController::GetTrackedCount()
    {
        return DebugViewPass::GetTrackedCount();
    }

    bool DebugViewController::Apply(ECSContext *)
    {
        return false;
    }

    void DebugViewController::Reset(ECSContext *world)
    {
        DebugViewPass::Reset(world);
    }
}
