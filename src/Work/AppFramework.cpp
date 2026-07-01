#include <hgl/framework/AppFramework.h>
#include <hgl/vk/VKInstance.h>
#include <hgl/vk/VKDeviceCreater.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GraphModuleManager.h>
#include <hgl/graph/module/SwapchainModule.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <hgl/graph/module/RenderTargetManager.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/ecs/core/DefaultSystems.h>
#include <hgl/log/Logger.h>
#include <hgl/io/event/MouseEvent.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/systems/tick/InputSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>

namespace hgl
{
    namespace graph
    {
        bool InitShaderCompiler();
        void CloseShaderCompiler();
    }

    namespace
    {
        static int APP_FRAMEWORK_COUNT = 0;

        graph::VulkanInstance *CreateVulkanInstance(const U8String &app_name)
        {
            graph::CreateInstanceLayerInfo cili;

            mem_zero(cili);

            cili.lunarg.standard_validation = true;
            cili.khronos.validation = true;

            graph::InitVulkanInstanceProperties();

            return CreateInstance(app_name, nullptr, &cili);
        }
    } // namespace

    AppFramework::AppFramework(const OSString &name)
        : app_name(name)
    {
    }

    AppFramework::~AppFramework()
    {
        GLogDebug("================ AppFramework Destructor Start ================");
        if (device)
            device->WaitIdle();

        // 1. Shutdown ECS context FIRST so systems can clean up their buffers
        // while managers are still alive
        GLogDebug("Step 1: Calling ECSContext::Shutdown()");
        if (default_ecs_context)
            default_ecs_context->Shutdown();
        GLogDebug("Step 1 complete");

        // 1.5 Destroy ECS context while GraphicsContext/BufferManager are still alive.
        // This allows system destructors to release UBO/VBO resources through managers.
        GLogDebug("Step 1.5: Deleting ECSContext");
        if (default_ecs_context)
        {
            delete default_ecs_context;
            default_ecs_context = nullptr;
        }
        GLogDebug("Step 1.5 complete");

        // 2. GraphicsContext shutdown - calls Release() on all modules (including sc_module)
        // GraphicsContext owns all modules through GraphModuleManager
        GLogDebug("Step 2: Calling GraphicsContext::Shutdown()");
        if (graphics_context)
            graphics_context->Shutdown();
        GLogDebug("Step 2 complete");

        // 3. Release render context
        render_context.reset();

        // 4. Clear graphics context (owns all managers and modules)
        // GraphModuleManager destructor automatically calls Release() on all modules
        // and then deletes them, so don't directly manage sc_module
        GLogDebug("Step 4: Deleting GraphicsContext");
        SAFE_CLEAR(graphics_context);
        GLogDebug("Step 4 complete");
        sc_module = nullptr;  // sc_module was deleted by GraphModuleManager, just null the pointer

        // 6. Wait for GPU to complete all operations before destroying device/window
        if (device)
        {
            device->WaitIdle();
        }

        // 7. Cleanup GPU resources
        GLogDebug("Step 7: Deleting VulkanDevice (will check for leaks)");
        SAFE_CLEAR(device);
        GLogDebug("Step 7 complete");
        SAFE_CLEAR(inst);
        SAFE_CLEAR(win);
        GLogDebug("================ AppFramework Destructor End ================");

        --APP_FRAMEWORK_COUNT;

        if (APP_FRAMEWORK_COUNT == 0)
        {
            graph::CloseShaderCompiler();
        }
    }

    io::EventProcResult AppFramework::OnEvent(const io::EventHeader &header, const uint64 data)
    {
        // Forward events to ECS InputSystem
        if (default_ecs_context)
        {
            auto input_sys = default_ecs_context->GetSystem<ecs::InputSystem>();
            if (input_sys)
            {
                auto *event_dispatcher = input_sys->GetEventDispatcher();
                if (event_dispatcher)
                    event_dispatcher->OnEvent(header, data);
            }
        }

        // Track mouse coords (for legacy compatibility)
        if (header.type == io::InputEventSource::Mouse)
        {
            if (io::MouseAction(header.id) == io::MouseAction::Move)
            {
                const io::MouseEventData *med = (const io::MouseEventData *)&data;

                mouse_coord.x = med->x;
                mouse_coord.y = med->y;
            }
        }

        return io::WindowEvent::OnEvent(header, data);
    }

    bool AppFramework::Init(uint w, uint h)
    {
        return Init(w, h, 0, nullptr);
    }

    bool AppFramework::Init(uint w, uint h, int argc, os_char **argv)
    {
        (void)argc;
        (void)argv;

        if (APP_FRAMEWORK_COUNT == 0)
        {
            if (!graph::InitShaderCompiler())
                return false;

            logger::InitLogger(app_name);

            InitNativeWindowSystem();
        }

        ++APP_FRAMEWORK_COUNT;

        // Create window
        win = CreateRenderWindow(app_name);
        if (!win)
            return false;

        if (!win->Create(w, h))
        {
            delete win;
            win = nullptr;
            return false;
        }

        // Create Vulkan instance
        const U8String u8_app_name = to_u8(app_name.c_str(), app_name.Length());

        inst = CreateVulkanInstance(u8_app_name);
        if (!inst)
            return false;

        // Create Vulkan device
        graph::VulkanHardwareRequirement vh_req;

        device = CreateRenderDevice(inst, win, &vh_req);
        if (!device)
            return false;

        win->AddChildDispatcher(this);

        // Create graphics context
        graphics_context = new graph::GraphicsContext(device);
        if (!graphics_context)
            return false;

        if (!graphics_context->Initialize())
            return false;

        // Create render context
        render_context = std::make_unique<graph::RenderContext>();

        // Create default ECS context early so modules can access it
        default_ecs_context = new ecs::ECSContext("DefaultECSWorld");

        if (default_ecs_context)
        {
            default_ecs_context->SetGraphicsContext(graphics_context);
        }

        // Create render target manager (needs ECS context)
        auto *rt_manager = new graph::RenderTargetManager(
            graphics_context,
            default_ecs_context,
            graphics_context->GetTextureManager());

        graphics_context->GetModuleManager()->Register(rt_manager);

        // Create swapchain module
        sc_module = new graph::SwapchainModule(
            graphics_context,
            default_ecs_context,
            graphics_context->GetTextureManager(),
            rt_manager);

        graphics_context->GetModuleManager()->Register(sc_module);

        // Setup render context
        if (render_context)
        {
            render_context->SetGraphicsContext(graphics_context);
            render_context->SetCurrentRenderTarget(GetSwapchainRenderTarget());
        }

        // Initialize ECS graphics and systems
        if (default_ecs_context)
        {
            default_ecs_context->InitializeGraphics(device, GetSwapchainRenderTarget());
            default_ecs_context->SetRenderContext(render_context.get());
        }

        // Register ECS systems
        if (default_ecs_context)
        {
            auto input_system = ecs::RegisterDefaultEcsSystems(default_ecs_context, GetSwapchainRenderTarget());
            if (input_system)
                AddChildDispatcher(input_system->GetEventDispatcher());

            default_ecs_context->Initialize();
        }

        return true;
    }

    void AppFramework::OnResize(uint w, uint h)
    {
        VkExtent2D ext(w, h);

        if (sc_module)
            sc_module->OnResize(ext);

        if (graphics_context)
            graphics_context->OnResize(ext);

        if (default_ecs_context)
        {
            // Update render target first
            default_ecs_context->SetRenderTarget(GetSwapchainRenderTarget());
            // Then notify all dependent systems via OnResize
            default_ecs_context->OnResize(ext);
        }

        if (render_context)
            render_context->SetCurrentRenderTarget(GetSwapchainRenderTarget());
    }

    void AppFramework::OnActive(bool)
    {
    }

    void AppFramework::OnClose()
    {
    }

    void AppFramework::Tick()
    {
    }

} // namespace hgl
