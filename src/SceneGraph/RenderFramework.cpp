#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/manager/RenderPassManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/Time.h>

VK_NAMESPACE_BEGIN

bool InitShaderCompiler();
void CloseShaderCompiler();

GraphModuleManager *InitGraphModuleManager(GPUDevice *dev);
bool ClearGraphModuleManager(GPUDevice *dev);

RenderFramework::RenderFramework()
{
    graph_module_manager=InitGraphModuleManager(device);

    render_pass_manager=graph_module_manager->GetModule<RenderPassManager>(true);
    swapchain_module=graph_module_manager->GetModule<SwapchainModule>(true);
}

RenderFramework::~RenderFramework()
{
//    if(swapchain_module)graph_module_manager->ReleaseModule(swapchain_module);

    ClearGraphModuleManager(device);

    CloseShaderCompiler();
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

    swapchain_module->Swap();
    last_time=cur_time;
    ++frame_count;
}

bool RenderFramework::Init()
{
    if(!InitShaderCompiler())
        return(false);

    return(true);
}

void RenderFramework::OnResize(uint w,uint h)
{
    viewport_info.Set(w,h);

    graph_module_manager->OnResize(VkExtent2D{.width=w,.height=h});
}

void RenderFramework::OnActive(bool)
{
}

void RenderFramework::OnClose()
{
}
VK_NAMESPACE_END
