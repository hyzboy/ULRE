#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/debug/DebugViewController.h>

int main()
{
    using hgl::ecs::DebugViewController;
    using hgl::ecs::ECSContext;
    using hgl::graph::mtl::DebugViewMode;

    ECSContext world("DebugViewOwner");
    if(!DebugViewController::RequestMode(DebugViewMode::WorldNormal)
       || !DebugViewController::Apply(&world)
       || DebugViewController::GetAppliedMode()!=DebugViewMode::WorldNormal)
        return 1;
    {
        ECSContext other("UnrelatedWorld");
        other.Shutdown();
        if(DebugViewController::GetAppliedMode()!=DebugViewMode::WorldNormal)
            return 2;
    }
    if(DebugViewController::GetAppliedMode()!=DebugViewMode::WorldNormal)
        return 3;
    world.Shutdown();
    if(DebugViewController::GetAppliedMode()!=DebugViewMode::Off
       || DebugViewController::GetRequestedMode()!=DebugViewMode::Off)
        return 4;
    return 0;
}
