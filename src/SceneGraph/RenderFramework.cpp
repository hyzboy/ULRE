#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/log/Logger.h>

VK_NAMESPACE_BEGIN

bool InitShaderCompiler();
void CloseShaderCompiler();

namespace 
{
    static int RENDER_FRAMEWORK_COUNT=0;

    hgl::graph::VulkanInstance *CreateVulkanInstance(const AnsiString &app_name)
    {
        CreateInstanceLayerInfo cili;

        hgl_zero(cili);

        cili.lunarg.standard_validation = true;
        cili.khronos.validation = true;

        InitVulkanInstanceProperties();

        return CreateInstance(app_name,nullptr,&cili);
    }
}//namespace

RenderFramework::RenderFramework(const OSString &an)
{
    app_name=an;


}

RenderFramework::~RenderFramework()
{
    SAFE_CLEAR(device_render_pass);
    SAFE_CLEAR(module_manager)

    --RENDER_FRAMEWORK_COUNT;

    if(RENDER_FRAMEWORK_COUNT==0)
    {
        CloseShaderCompiler();
    }
}

bool RenderFramework::Init(uint w,uint h)
{
    if(RENDER_FRAMEWORK_COUNT==0)
    {
        if(!InitShaderCompiler())
            return(false);

        logger::InitLogger(app_name);

        InitNativeWindowSystem();
    }

    ++RENDER_FRAMEWORK_COUNT;

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
    
    VulkanHardwareRequirement vh_req;

    device=CreateRenderDevice(inst,win,&vh_req);

    if(!device)
        return(false);

    win->Join(this);

    OnResize(w,h);

    module_manager=new GraphModuleManager(device);

    render_pass_manager=module_manager->GetOrCreate<RenderPassManager>();

    if(!render_pass_manager)
        return(false);

    {
        auto *attr=GetDevice()->GetDeviceAttribute();

        SwapchainRenderbufferInfo rbi(attr->surface_format.format,attr->physical_device->GetDepthFormat());

        device_render_pass=render_pass_manager->AcquireRenderPass(&rbi);

        if(!device_render_pass)
            return(false);

        #ifdef _DEBUG
            if(attr->debug_utils)
                attr->debug_utils->SetRenderPass(device_render_pass->GetVkRenderPass(),"MainDeviceRenderPass");
        #endif//_DEBUG
    }

    return(true);
}

void RenderFramework::OnResize(uint w,uint h)
{
    io::WindowEvent::OnResize(w,h);
}

void RenderFramework::OnActive(bool)
{
}

void RenderFramework::OnClose()
{
}
VK_NAMESPACE_END
