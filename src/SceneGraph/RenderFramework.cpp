#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/Time.h>

VK_NAMESPACE_BEGIN

GraphModuleManager *InitGraphModuleManager(GPUDevice *dev);
bool ClearGraphModuleManager(GPUDevice *dev);

RenderFramework::RenderFramework()
{
    graph_module_manager=InitGraphModuleManager(device);

    swapchain_module=graph_module_manager->GetModule<SwapchainModule>(true);
}

RenderFramework::~RenderFramework()
{
//    if(swapchain_module)graph_module_manager->ReleaseModule(swapchain_module);

    ClearGraphModuleManager(device);
}

void RenderFramework::StartTime()
{
    last_time=cur_time=GetDoubleTime();
    frame_count++;
}

void RenderFramework::MainLoop()
{
    cur_time=GetDoubleTime();

    const double delta_time=cur_time-last_time;

    for(auto rm:module_list)
    {
        if(rm->IsEnable())
            rm->OnPreFrame();
    }

    BeginFrame();

    for(auto rm:module_list)
    {
        if(rm->IsEnable())
            rm->OnExecute(delta_time,nullptr);
    }

    for(auto rm:module_list)
    {
        if(rm->IsEnable())
            rm->OnPostFrame();
    }

    EndFrame();
    last_time=cur_time;
    ++frame_count;
}

VK_NAMESPACE_END
