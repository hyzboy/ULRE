#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/RenderModule.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/FirstPersonCameraControl.h>
#include<hgl/graph/Renderer.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/log/Logger.h>
#include<hgl/Time.h>

VK_NAMESPACE_BEGIN

bool InitShaderCompiler();
void CloseShaderCompiler();

void InitializeComponentManager();
void UninitializeComponentManager();

namespace mtl
{
    void ClearMaterialFactory();
}

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
    SAFE_CLEAR(default_renderer)
    SAFE_CLEAR(default_camera_control)
    SAFE_CLEAR(default_camera)
    SAFE_CLEAR(default_scene)
    SAFE_CLEAR(render_resource)
    SAFE_CLEAR(module_manager)

    --RENDER_FRAMEWORK_COUNT;

    if(RENDER_FRAMEWORK_COUNT==0)
    {
        UninitializeComponentManager();
        STD_MTL_NAMESPACE::ClearMaterialFactory();
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

        InitializeComponentManager();
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

    win->RegistryEventDispatch(this);

    module_manager=new GraphModuleManager(this);

    rp_manager=module_manager->GetOrCreate<RenderPassManager>();

    if(!rp_manager)
        return(false);

    tex_manager=module_manager->GetOrCreate<TextureManager>();

    if(!tex_manager)
        return(false);

    rt_manager=new RenderTargetManager(this,tex_manager,rp_manager);
    module_manager->Registry(rt_manager);

    sc_module=new SwapchainModule(this,tex_manager,rt_manager,rp_manager);
    module_manager->Registry(sc_module);

    render_resource=new RenderResource(device);

    default_scene=new Scene;

    default_camera=new Camera();

    CreateDefaultRenderer();

    return(true);
}

void RenderFramework::CreateDefaultRenderer()
{
    SAFE_CLEAR(default_renderer)

    IRenderTarget *rt=GetSwapchainRenderTarget();

    default_renderer=new Renderer(rt);
    default_renderer->SetScene(default_scene);

    if(!default_camera_control)
    {
        auto ubo_camera_info=device->CreateUBO<UBOCameraInfo>(&mtl::SBS_CameraInfo);

        auto fpcc=new FirstPersonCameraControl(rt->GetViewportInfo(),default_camera,ubo_camera_info);

        auto ckc=new CameraKeyboardControl(fpcc);
        auto cmc=new CameraMouseControl(fpcc);

        this->RegistryEventDispatch(ckc);
        this->RegistryEventDispatch(cmc);

        default_camera_control=fpcc;

        mouse_event=cmc;
    }

    default_renderer->SetCameraControl(default_camera_control);
}

void RenderFramework::OnResize(uint w,uint h)
{
    VkExtent2D ext(w,h);

    sc_module->OnResize(ext);        //其实swapchain_module并不需要传递尺寸数据过去

    CreateDefaultRenderer();
}

void RenderFramework::OnActive(bool)
{
}

void RenderFramework::OnClose()
{
}

void RenderFramework::Tick()
{
    if(default_camera_control)
    {
        default_camera_control->Refresh();
    }
}

graph::Primitive *RenderFramework::CreatePrimitive( const AnsiString &name,
                                                      const uint32_t vertices_count,
                                                      const graph::VIL *vil,
                                                      const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
{
    auto *pc=new graph::PrimitiveCreater(GetDevice(),vil);

    pc->Init(name,vertices_count);

    for(const auto &vad:vad_list)
    {
        if(!pc->WriteVAB(vad.name,vad.format,vad.data))
        {
            delete pc;
            return(nullptr);
        }
    }

    auto *prim=pc->Create();

    if(prim)
        render_resource->Add(prim);

    return prim;
}

graph::Mesh *RenderFramework::CreateMesh( const AnsiString &name,
                                    const uint32_t vertices_count,
                                    graph::MaterialInstance *mi,
                                    graph::Pipeline *pipeline,
                                    const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
{
    auto *prim=this->CreatePrimitive(name,vertices_count,mi->GetVIL(),vad_list);

    if(!prim)
        return(nullptr);

    return render_resource->CreateMesh(prim,mi,pipeline);
}
VK_NAMESPACE_END
