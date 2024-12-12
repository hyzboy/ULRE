#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/manager/RenderPassManager.h>
#include<hgl/graph/manager/TextureManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/Time.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN

bool InitShaderCompiler();
void CloseShaderCompiler();

GraphModuleManager *InitGraphModuleManager(GPUDevice *dev);
bool ClearGraphModuleManager(GPUDevice *dev);

namespace
{
    hgl::graph::VulkanInstance *CreateVulkanInstance(const AnsiString &app_name)
    {
        CreateInstanceLayerInfo cili;

        hgl_zero(cili);

        cili.lunarg.standard_validation = true;
        cili.khronos.validation = true;

        InitVulkanInstanceProperties();

        return CreateInstance("VulkanTest",nullptr,&cili);
    }
}//namespace

RenderFramework::RenderFramework()
{
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
}

void RenderFramework::BeginFrame()
{
    cur_time=GetDoubleTime();    

    for(GraphModule *rm:per_frame_module_list)
    {
        if(rm->IsEnable())
            rm->OnPreFrame();
    }

    swapchain_module->BeginFrame();
}

void RenderFramework::EndFrame()
{
    swapchain_module->EndFrame();
    
    for(GraphModule *rm:per_frame_module_list)
    {
        if(rm->IsEnable())
            rm->OnPostFrame();
    }

    last_time=cur_time;
    ++frame_count;
}

void RenderFramework::MainLoop()
{
    const double delta_time=cur_time-last_time;

    BeginFrame();

    RenderCmdBuffer *rcb=swapchain_module->GetRenderCmdBuffer();

    if(rcb)
    {
        rcb->Begin();
        rcb->BindFramebuffer(swapchain_module->GetRenderPass(),swapchain_module->GetRenderTarget()->GetFramebuffer());

        for(RenderModule *rm:render_module_list)
        {
            if(rm->IsEnable())
                rm->OnFrameRender(delta_time,rcb);
        }

        rcb->End();
    }

    EndFrame();
}

void RenderFramework::Run()
{
    if(!win)return;
    if(!swapchain_module)return;

    while(win->Update())
    {
        if(win->IsVisible())
            MainLoop();

        device->WaitIdle();
    }
}

bool RenderFramework::Init(uint w,uint h,const OSString &app_name)
{
    logger::InitLogger(app_name);

    if(!InitShaderCompiler())
        return(false);

    InitNativeWindowSystem();

    win=CreateRenderWindow(app_name);
    if(!win)
        return(false);

    if(!win->Create(w,h))
    {
        delete win;
        win=nullptr;
        return(false);
    }

    inst=CreateVulkanInstance(ToAnsiString(app_name));
    if(!inst)
        return(false);

    {
        VulkanHardwareRequirement vh_req;

        device=CreateRenderDevice(inst,win,&vh_req);

        if(!device)
            return(false);

        graph_module_manager=InitGraphModuleManager(device,this);

        render_pass_manager =graph_module_manager->GetModule<RenderPassManager>(true);
        texture_manager     =graph_module_manager->GetModule<TextureManager>(true);
        swapchain_module    =graph_module_manager->GetModule<SwapchainModule>(true);
    }

    win->Join(this);

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
