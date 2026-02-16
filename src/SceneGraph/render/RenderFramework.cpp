#include<hgl/graph/render/RenderFramework.h>
#include<hgl/vk/VKInstance.h>
#include<hgl/vk/VKDeviceCreater.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsModule.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/log/Logger.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/systems/render/RenderBufferCommitSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/TextRenderSystem.h>
#include<hgl/ecs/systems/render/TextRenderSubmitSystem.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

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
    // 1. Disable graphics context before cleanup
    if(module_manager)
        module_manager->SetGraphicsContext(nullptr);

    // 2. Shutdown ECS context FIRST so systems can clean up their buffers
    // while BufferManager is still alive
    if(default_ecs_context)
    {
        default_ecs_context->Shutdown();
        delete default_ecs_context;
        default_ecs_context=nullptr;
    }

    // 3. Clear modules (GraphModuleManager destructor automatically calls Release() on all modules)
    // This ensures all GPU resources in each module are cleaned up before module deletion
    SAFE_CLEAR(module_manager)

    // 4. Release render context
    render_context.reset();

    // 5. Wait for GPU to complete all operations before destroying device/window
    if(device)
    {
        device->WaitIdle();
    }

    // 6. Cleanup GPU resources
    SAFE_CLEAR(device);
    SAFE_CLEAR(inst);
    SAFE_CLEAR(win);

    --RENDER_FRAMEWORK_COUNT;

    if(RENDER_FRAMEWORK_COUNT==0)
    {
        STD_MTL_NAMESPACE::ClearMaterialFactory();
        CloseShaderCompiler();
    }
}

io::EventProcResult RenderFramework::OnEvent(const io::EventHeader &header,const uint64 data)
{
    // 转发事件给ECS的InputSystem
    if(default_ecs_context)
    {
        auto input_sys = default_ecs_context->GetSystem<ecs::InputSystem>();
        if(input_sys)
        {
            auto* event_dispatcher = input_sys->GetEventDispatcher();
            if(event_dispatcher)
                event_dispatcher->OnEvent(header, data);
        }
    }

    // 保留原有逻辑(用于兼容)
    if(header.type == io::InputEventSource::Mouse)
    {
        // 使用MouseAction替代MouseAction / Use MouseAction instead of MouseAction
        if(io::MouseAction(header.id) == io::MouseAction::Move)
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

    module_manager=new GraphModuleManager();

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

    render_context = std::make_unique<graph::RenderContext>(
        device,
        tex_manager,
        buffer_manager,
        material_manager,
        sampler_manager,
        rp_manager,
        geometry_manager,
        primitive_manager);

    // create default ECS context early so modules can access it
    default_ecs_context = new ecs::ECSContext("DefaultECSWorld");

    std::shared_ptr<graph::GraphicsModule> graphics_ctx;

    if (default_ecs_context)
    {
        graphics_ctx = std::make_shared<graph::GraphicsModule>(device,
                                                               rp_manager,
                                                               tex_manager,
                                                               material_manager,
                                                               buffer_manager,
                                                               sampler_manager,
                                                               geometry_manager,
                                                               primitive_manager);

        graphics_ctx->SetLegacyRenderFramework(this);
        default_ecs_context->SetGraphicsContext(graphics_ctx);

        if(module_manager)
            module_manager->SetGraphicsContext(graphics_ctx.get());
    }

    rt_manager=new RenderTargetManager(graphics_ctx.get(),default_ecs_context,tex_manager,rp_manager);
    module_manager->Register(rt_manager);

    sc_module=new SwapchainModule(graphics_ctx.get(),default_ecs_context,tex_manager,rt_manager,rp_manager);
    module_manager->Register(sc_module);

    if (graphics_ctx)
    {
        graphics_ctx->SetDefaultRenderPass(GetDefaultRenderPass());

        default_ecs_context->InitializeGraphics(device, GetSwapchainRenderTarget());
        default_ecs_context->SetRenderContext(render_context.get());
    }

    if (render_context)
        render_context->SetCurrentRenderTarget(GetSwapchainRenderTarget());

    if(default_ecs_context)
    {
        auto text_render_system = default_ecs_context->RegisterTickSystem<ecs::TextRenderSystem>();
        auto environment_system = default_ecs_context->RegisterRenderSystem<ecs::EnvironmentSystem>();
        auto camera_system = default_ecs_context->RegisterTickSystem<ecs::CameraSystem>();
        auto render_target_system = default_ecs_context->RegisterRenderSystem<ecs::RenderTargetSystem>();
        auto render_collect_system = default_ecs_context->RegisterTickSystem<ecs::RenderPrimitiveCollectSystem>();
        auto render_batch_system = default_ecs_context->RegisterTickSystem<ecs::RenderPrimitiveBatchSystem>();
        auto render_commit_system = default_ecs_context->RegisterRenderSystem<ecs::RenderBufferCommitSystem>();
        auto render_submit_system = default_ecs_context->RegisterRenderSystem<ecs::RenderPrimitiveSubmitSystem>();
        auto text_submit_system = default_ecs_context->RegisterRenderSystem<ecs::TextRenderSubmitSystem>();
        auto line_render_system = default_ecs_context->RegisterRenderSystem<ecs::LineRenderSystem>();

        if (text_render_system)
        {
            text_render_system->SetWorld(default_ecs_context);
            text_render_system->SetRenderContext(default_ecs_context->GetRenderContext());
        }

        if (environment_system)
            environment_system->SetRenderContext(default_ecs_context->GetRenderContext());

        if (camera_system)
        {
            camera_system->SetRenderContext(default_ecs_context->GetRenderContext());
            IRenderTarget *default_rt = GetSwapchainRenderTarget();
            camera_system->SetViewportInfo(default_rt ? default_rt->GetViewportInfo() : nullptr);
        }

        if (render_target_system)
        {
            render_target_system->SetRenderContext(default_ecs_context->GetRenderContext());
            render_target_system->SetRenderTarget(GetSwapchainRenderTarget());
        }

        const CameraInfo* camera_info = camera_system ? camera_system->GetCameraInfo() : nullptr;

        render_collect_system->SetWorld(default_ecs_context);
        render_collect_system->SetCameraInfo(camera_info);

        render_batch_system->SetWorld(default_ecs_context);
        render_batch_system->SetDevice(device);
        render_batch_system->SetCameraInfo(camera_info);

        if (render_commit_system)
        {
            render_commit_system->SetWorld(default_ecs_context);
            render_commit_system->SetDevice(device);
        }

        render_submit_system->SetWorld(default_ecs_context);

        if (text_submit_system)
            text_submit_system->SetWorld(default_ecs_context);

        if (line_render_system)
        {
            line_render_system->SetRenderContext(default_ecs_context->GetRenderContext());
            line_render_system->SetRenderTarget(GetSwapchainRenderTarget());
        }

        auto input_system=default_ecs_context->RegisterTickSystem<ecs::InputSystem>();

        AddChildDispatcher(input_system->GetEventDispatcher());

        default_ecs_context->Initialize();
    }

    return(true);
}

void RenderFramework::OnResize(uint w,uint h)
{
    VkExtent2D ext(w,h);

    sc_module->OnResize(ext);        //其实swapchain_module并不需要传递尺寸数据过去

    if (default_ecs_context)
    {
        auto render_target_system = default_ecs_context->GetSystem<ecs::RenderTargetSystem>();
        if (render_target_system)
            render_target_system->SetRenderTarget(GetSwapchainRenderTarget());
    }

    if (render_context)
        render_context->SetCurrentRenderTarget(GetSwapchainRenderTarget());
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

LineRenderManager *RenderFramework::GetLineRenderManager() const
{
    if (!default_ecs_context)
        return nullptr;

    auto line_system = default_ecs_context->GetSystem<ecs::LineRenderSystem>();
    return line_system ? line_system->GetLineRenderManager() : nullptr;
}

graph::VertexDataManager *RenderFramework::CreateVDM(const graph::VIL *vil,const VkDeviceSize vertices_number,VkDeviceSize indices_number,const IndexType type)
{
    if(!vil||vertices_number<=0||indices_number<=0||!device->IsSupport(type))
        return(nullptr);

    auto *vdm=new VertexDataManager(buffer_manager,vil);

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
    auto *pc=new graph::GeometryCreater(GetDevice(),vil,buffer_manager);

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

