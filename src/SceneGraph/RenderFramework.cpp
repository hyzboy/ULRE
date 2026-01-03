#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/World.h>
#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/log/Logger.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/ecs/RenderPrimitiveSystem.h>

COMPONENT_NAMESPACE_BEGIN
void InitializeComponentManager();
void UninitializeComponentManager();
COMPONENT_NAMESPACE_END

VK_NAMESPACE_BEGIN

bool InitShaderCompiler();
void CloseShaderCompiler();

namespace mtl
{
    void ClearMaterialFactory();
}

namespace 
{
    static int RENDER_FRAMEWORK_COUNT=0;

    hgl::graph::VulkanInstance *CreateVulkanInstance(const U8String &app_name)
    {
        CreateInstanceLayerInfo cili;

        mem_zero(cili);

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
    SAFE_CLEAR(default_world)
    if(default_ecs_context)
    {
        default_ecs_context->Shutdown();
        delete default_ecs_context;
        default_ecs_context=nullptr;
    }
    SAFE_CLEAR(default_scene_renderer);
    SAFE_CLEAR(module_manager)

    --RENDER_FRAMEWORK_COUNT;

    if(RENDER_FRAMEWORK_COUNT==0)
    {
        UninitializeComponentManager();
        STD_MTL_NAMESPACE::ClearMaterialFactory();
        CloseShaderCompiler();
    }
}

io::EventProcResult RenderFramework::OnEvent(const io::EventHeader &header,const uint64 data)
{
    if(header.type == io::InputEventSource::Mouse)
    {
        if(io::MouseEventID(header.id) == io::MouseEventID::Move)
        {
            const io::MouseEventData *med=(const io::MouseEventData *)&data;

            mouse_coord.x=med->x;
            mouse_coord.y=med->y;
        }        
    }

    return io::WindowEvent::OnEvent(header,data);
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

    const U8String u8_app_name=to_u8(app_name.c_str(),app_name.Length());

    inst=CreateVulkanInstance(u8_app_name);
    if(!inst)
        return(false);
    
    VulkanHardwareRequirement vh_req;

    device=CreateRenderDevice(inst,win,&vh_req);

    if(!device)
        return(false);

    win->AddChildDispatcher(this);

    module_manager=new GraphModuleManager(this);

    rp_manager=module_manager->GetOrCreate<RenderPassManager>();
    if(!rp_manager)
        return(false);

    tex_manager=module_manager->GetOrCreate<TextureManager>();
    if(!tex_manager)
        return(false);

    sampler_manager=module_manager->GetOrCreate<SamplerManager>();
    if(!sampler_manager)
        return(false);

    geometry_manager=module_manager->GetOrCreate<GeometryManager>();
    if(!geometry_manager)
        return(false);

    primitive_manager=module_manager->GetOrCreate<PrimitiveManager>();
    if(!primitive_manager)
        return(false);

    material_manager=module_manager->GetOrCreate<MaterialManager>();
    if(!material_manager)
        return(false);

    buffer_manager=module_manager->GetOrCreate<BufferManager>();
    if(!buffer_manager)
        return(false);

    rt_manager=new RenderTargetManager(this,tex_manager,rp_manager);
    module_manager->Register(rt_manager);

    sc_module=new SwapchainModule(this,tex_manager,rt_manager,rp_manager);
    module_manager->Register(sc_module);

    OnChangeDefaultWorld(new World(this));
    
    // create default ECS context BEFORE creating SceneRenderer
    default_ecs_context = new ecs::ECSContext("DefaultECSWorld");
    
    CreateDefaultSceneRenderer();

    if(default_ecs_context)
    {
        auto render_primitive_system=default_ecs_context->RegisterRenderSystem<ecs::RenderPrimitiveSystem>();

        render_primitive_system->SetDevice(device);
        render_primitive_system->SetWorld(default_ecs_context);
        render_primitive_system->SetCameraInfo(default_scene_renderer->GetCameraInfo());

        default_ecs_context->Initialize();
    }

    return(true);
}

void RenderFramework::OnChangeDefaultWorld(World *s)
{
    if(default_world==s)
        return;

    default_world=s;
}

void RenderFramework::CreateDefaultSceneRenderer()
{
    IRenderTarget *rt = GetSwapchainRenderTarget();

    if(!default_scene_renderer)
    {
        default_scene_renderer=new SceneRenderer(this,rt);

        this->AddChildDispatcher(default_scene_renderer);

        default_scene_renderer->SetWorld(default_world);
        default_scene_renderer->SetECSContext(default_ecs_context);
    }
    else
    {
        default_scene_renderer->SetRenderTarget(rt);
        default_scene_renderer->SetECSContext(default_ecs_context);
    }
}

void RenderFramework::OnResize(uint w,uint h)
{
    VkExtent2D ext(w,h);

    sc_module->OnResize(ext);        //其实swapchain_module并不需要传递尺寸数据过去

    CreateDefaultSceneRenderer();
}

void RenderFramework::OnActive(bool)
{
}

void RenderFramework::OnClose()
{
}

void RenderFramework::Tick()
{
}

graph::VertexDataManager *RenderFramework::CreateVDM(const graph::VIL *vil,const VkDeviceSize vertices_number,VkDeviceSize indices_number,const IndexType type)
{
    if(!vil||vertices_number<=0||indices_number<=0||!device->IsSupport(type))
        return(nullptr);

    auto *vdm=new VertexDataManager(device,vil);

    if(!vdm)
        return(nullptr);

    if(!vdm->Init(vertices_number,indices_number,type))
    {
        delete vdm;
        return nullptr;
    }

    return vdm;
}

graph::Geometry *RenderFramework::CreateGeometry( const AnsiString &name,
                                                    const uint32_t vertices_count,
                                                    const graph::VIL *vil,
                                                    const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
{
    auto *pc=new graph::GeometryCreater(GetDevice(),vil);

    pc->Init(name,vertices_count);

    for(const auto &vad:vad_list)
    {
        if(!pc->WriteVAB(vad.name,vad.format,vad.data))
        {
            delete pc;
            return(nullptr);
        }
    }

    auto *geometry=pc->Create();

    if(geometry)
        geometry_manager->Add(geometry);

    return geometry;
}

graph::Primitive *RenderFramework::CreatePrimitive(   const AnsiString &name,
                                            const uint32_t vertices_count,
                                            graph::MaterialInstance *mi,
                                            graph::Pipeline *pipeline,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
{
    auto *geometry=this->CreateGeometry(name,vertices_count,mi->GetVIL(),vad_list);

    if(!geometry)
        return(nullptr);

    // Prefer PrimitiveManager to create and own meshes
    return primitive_manager->CreatePrimitive(geometry,mi,pipeline);
}
VK_NAMESPACE_END
