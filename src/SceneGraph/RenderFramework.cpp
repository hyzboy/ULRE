#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/RenderModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/log/Logger.h>
#include<hgl/Time.h>

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
    SAFE_CLEAR(render_resource)
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

    module_manager=new GraphModuleManager(device);

    rp_manager=module_manager->GetOrCreate<RenderPassManager>();

    if(!rp_manager)
        return(false);

    tex_manager=module_manager->GetOrCreate<TextureManager>();

    if(!tex_manager)
        return(false);

    rt_manager=new RenderTargetManager(device,tex_manager,rp_manager);
    module_manager->Registry(rt_manager);

    sc_module=new SwapchainModule(device,tex_manager,rt_manager,rp_manager);
    module_manager->Registry(sc_module);

    OnResize(w,h);

    render_resource=new RenderResource(device);

    return(true);
}

void RenderFramework::OnResize(uint w,uint h)
{
    io::WindowEvent::OnResize(w,h);

    VkExtent2D ext(w,h);

    sc_module->OnResize(ext);        //其实swapchain_module并不需要传递尺寸数据过去
}

void RenderFramework::OnActive(bool)
{
}

void RenderFramework::OnClose()
{
}

VK_NAMESPACE_END
