#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/VKInstance.h>
#include<hgl/graph/VKDeviceCreater.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MeshManager.h>
#include<hgl/graph/VKRenderTargetSwapchain.h>
#include<hgl/graph/module/RenderModule.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/VertexDataManager.h>
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

    hgl::graph::VulkanInstance *CreateVulkanInstance(const U8String &app_name)
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
    SAFE_CLEAR(default_scene)
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

    primitive_manager=module_manager->GetOrCreate<PrimitiveManager>();
    if(!primitive_manager)
        return(false);

    mesh_manager=module_manager->GetOrCreate<MeshManager>();
    if(!mesh_manager)
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

    OnChangeDefaultScene(new Scene(this));

    CreateDefaultSceneRenderer();

    return(true);
}

void RenderFramework::OnChangeDefaultScene(Scene *s)
{
    if(default_scene==s)
        return;

    default_scene=s;
}

void RenderFramework::CreateDefaultSceneRenderer()
{
    IRenderTarget *rt = GetSwapchainRenderTarget();

    if(!default_scene_renderer)
    {
        default_scene_renderer=new SceneRenderer(this,rt);

        this->AddChildDispatcher(default_scene_renderer);

        default_scene_renderer->SetScene(default_scene);
    }
    else
    {
        default_scene_renderer->SetRenderTarget(rt);
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
        primitive_manager->Add(prim);

    return prim;
}

graph::SubMesh *RenderFramework::CreateSubMesh(   const AnsiString &name,
                                            const uint32_t vertices_count,
                                            graph::MaterialInstance *mi,
                                            graph::Pipeline *pipeline,
                                            const std::initializer_list<graph::VertexAttribDataPtr> &vad_list)
{
    auto *prim=this->CreatePrimitive(name,vertices_count,mi->GetVIL(),vad_list);

    if(!prim)
        return(nullptr);

    // Prefer MeshManager to create and own meshes
    return mesh_manager->CreateSubMesh(prim,mi,pipeline);
}
VK_NAMESPACE_END
