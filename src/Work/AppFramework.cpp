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
#include <hgl/io/event/KeyboardEvent.h>
#include <hgl/ecs/components/TextComponent.h>
#include <hgl/graph/font/FontSource.h>
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

        const char *BindSlotSummaryLogModeName(ecs::BindSlotSummaryLogMode mode)
        {
            switch (mode)
            {
                case ecs::BindSlotSummaryLogMode::Off:        return "Off";
                case ecs::BindSlotSummaryLogMode::Throttled:  return "Throttled";
                case ecs::BindSlotSummaryLogMode::EveryFrame: return "EveryFrame";
                default:                                       return "Unknown";
            }
        }

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

        ecs_debug_hud_text.reset();
        if (ecs_debug_hud_font)
        {
            delete ecs_debug_hud_font;
            ecs_debug_hud_font = nullptr;
        }

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

        // 5. Destroy ECS context after graphics resources are cleaned up
        GLogDebug("Step 5: Deleting ECSContext");
        if (default_ecs_context)
        {
            delete default_ecs_context;
            default_ecs_context = nullptr;
        }
        GLogDebug("Step 5 complete");

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
        else if (header.type == io::InputEventSource::Keyboard)
        {
            io::KeyboardEventID event_id = io::KeyboardEventID(header.id);
            const io::KeyboardEventData *ked = (const io::KeyboardEventData *)&data;
            const io::KeyboardButton key = io::KeyboardButton(ked->key);

            if (key == io::KeyboardButton::F7)
            {
                if (event_id == io::KeyboardEventID::Pressed)
                {
                    if (!debug_hud_toggle_key_pressed)
                    {
                        debug_hud_toggle_key_pressed = true;
                        SetEcsDebugHudVisible(!IsEcsDebugHudVisible());
                    }
                }
                else if (event_id == io::KeyboardEventID::Released)
                {
                    debug_hud_toggle_key_pressed = false;
                }
            }

            if (key == io::KeyboardButton::F8)
            {
                if (event_id == io::KeyboardEventID::Pressed)
                {
                    if (!bind_slot_summary_toggle_key_pressed)
                    {
                        bind_slot_summary_toggle_key_pressed = true;
                        CycleBindSlotSummaryLogMode();
                    }
                }
                else if (event_id == io::KeyboardEventID::Released)
                {
                    bind_slot_summary_toggle_key_pressed = false;
                }
            }

            if (key == io::KeyboardButton::F9)
            {
                if (event_id == io::KeyboardEventID::Pressed)
                {
                    if (!descriptor_diag_toggle_key_pressed)
                    {
                        descriptor_diag_toggle_key_pressed = true;
                        ToggleDescriptorContractDiagLogEnabled();
                    }
                }
                else if (event_id == io::KeyboardEventID::Released)
                {
                    descriptor_diag_toggle_key_pressed = false;
                }
            }

            if (key == io::KeyboardButton::F10)
            {
                if (event_id == io::KeyboardEventID::Pressed)
                {
                    if (!material_query_toggle_key_pressed)
                    {
                        material_query_toggle_key_pressed = true;
                        ToggleMaterialBindingQueryLogEnabled();
                    }
                }
                else if (event_id == io::KeyboardEventID::Released)
                {
                    material_query_toggle_key_pressed = false;
                }
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
            auto systems = ecs::RegisterDefaultEcsSystems(default_ecs_context,
                                                          GetSwapchainRenderTarget(),
                                                          &default_ecs_debug_config);
            if (systems.input_system)
                AddChildDispatcher(systems.input_system->GetEventDispatcher());

            default_ecs_context->Initialize();
            InitializeEcsDebugHud();
        }

        return true;
    }

    void AppFramework::SetDefaultEcsDebugConfig(const ecs::DefaultEcsDebugConfig &config)
    {
        default_ecs_debug_config = config;

        if (default_ecs_context)
        {
            ecs::ApplyDefaultEcsDebugConfig(default_ecs_context, default_ecs_debug_config);
        }

        UpdateEcsDebugHudText();
    }

    void AppFramework::SetBindSlotSummaryLogMode(ecs::BindSlotSummaryLogMode mode)
    {
        default_ecs_debug_config.bind_slot_summary_log_mode = mode;

        if (default_ecs_context)
        {
            default_ecs_context->SetBindSlotSummaryLogMode(mode);
        }

        GLogInfo("[ECS Debug] BindSlotSummaryLogMode set to %s", BindSlotSummaryLogModeName(mode));
        UpdateEcsDebugHudText();
    }

    void AppFramework::CycleBindSlotSummaryLogMode()
    {
        const ecs::BindSlotSummaryLogMode current = GetBindSlotSummaryLogMode();
        ecs::BindSlotSummaryLogMode next = ecs::BindSlotSummaryLogMode::Throttled;

        switch (current)
        {
            case ecs::BindSlotSummaryLogMode::Off:
                next = ecs::BindSlotSummaryLogMode::Throttled;
                break;
            case ecs::BindSlotSummaryLogMode::Throttled:
                next = ecs::BindSlotSummaryLogMode::EveryFrame;
                break;
            case ecs::BindSlotSummaryLogMode::EveryFrame:
                next = ecs::BindSlotSummaryLogMode::Off;
                break;
            default:
                next = ecs::BindSlotSummaryLogMode::Throttled;
                break;
        }

        SetBindSlotSummaryLogMode(next);
    }

    void AppFramework::SetDescriptorContractDiagLogEnabled(bool enabled)
    {
        default_ecs_debug_config.descriptor_contract_diag_log_enabled = enabled;

        if (default_ecs_context)
        {
            default_ecs_context->SetDescriptorContractDiagnosticsLogEnabled(enabled);
        }

        GLogInfo("[ECS Debug] DescriptorContractDiagLog set to %s", enabled ? "On" : "Off");
        UpdateEcsDebugHudText();
    }

    void AppFramework::ToggleDescriptorContractDiagLogEnabled()
    {
        SetDescriptorContractDiagLogEnabled(!IsDescriptorContractDiagLogEnabled());
    }

    void AppFramework::SetMaterialBindingQueryLogEnabled(bool enabled)
    {
        default_ecs_debug_config.material_binding_query_log_enabled = enabled;

        if (default_ecs_context)
        {
            default_ecs_context->SetMaterialBindingQueryLogEnabled(enabled);
        }

        GLogInfo("[ECS Debug] MaterialBindingQueryLog set to %s", enabled ? "On" : "Off");
        UpdateEcsDebugHudText();
    }

    void AppFramework::ToggleMaterialBindingQueryLogEnabled()
    {
        SetMaterialBindingQueryLogEnabled(!IsMaterialBindingQueryLogEnabled());
    }

    void AppFramework::SetEcsDebugHudVisible(bool visible)
    {
        ecs_debug_hud_visible = visible;
        GLogInfo("[ECS Debug] HUD %s (F7 toggle, F8/F9/F10 controls)", ecs_debug_hud_visible ? "enabled" : "disabled");
        UpdateEcsDebugHudText();
    }

    void AppFramework::InitializeEcsDebugHud()
    {
        if (!default_ecs_context)
            return;

        if (!ecs_debug_hud_font)
            ecs_debug_hud_font = graph::CreateFontSource(OS_TEXT("Consolas"), 20);

        if (!ecs_debug_hud_font)
            return;

        auto *entity = default_ecs_context->CreateEntity();
        if (!entity)
            return;

        ecs_debug_hud_text = entity->AddComponent<ecs::TextComponent>();
        if (!ecs_debug_hud_text)
            return;

        graph::layout::CharStyle style;
        style.CharColor.r = 1.0f;
        style.CharColor.g = 1.0f;
        style.CharColor.b = 0.0f;
        style.CharColor.a = 1.0f;

        ecs_debug_hud_text->SetFontSource(ecs_debug_hud_font);
        ecs_debug_hud_text->SetStartPosition({16, 16});
        ecs_debug_hud_text->SetCharStyle(style);

        UpdateEcsDebugHudText();
    }

    void AppFramework::UpdateEcsDebugHudText()
    {
        if (!ecs_debug_hud_text)
            return;

        if (!ecs_debug_hud_visible)
        {
            ecs_debug_hud_text->SetText(U16String());
            return;
        }

        const u16char *mode_text = U16_TEXT("Unknown");

        switch (GetBindSlotSummaryLogMode())
        {
            case ecs::BindSlotSummaryLogMode::Off:
                mode_text = U16_TEXT("Off");
                break;
            case ecs::BindSlotSummaryLogMode::Throttled:
                mode_text = U16_TEXT("Throttled");
                break;
            case ecs::BindSlotSummaryLogMode::EveryFrame:
                mode_text = U16_TEXT("EveryFrame");
                break;
            default:
                mode_text = U16_TEXT("Unknown");
                break;
        }

        U16String hud_text = U16_TEXT("ECS Debug HUD");
        hud_text += U16_TEXT("\nBindSlotSummary: ");
        hud_text += mode_text;
        hud_text += U16_TEXT("\nDescriptorContractDiag: ");
        hud_text += IsDescriptorContractDiagLogEnabled() ? U16_TEXT("On") : U16_TEXT("Off");
        hud_text += U16_TEXT("\nMaterialBindingQuery: ");
        hud_text += IsMaterialBindingQueryLogEnabled() ? U16_TEXT("On") : U16_TEXT("Off");
        hud_text += U16_TEXT("\nF7: Hide HUD");
        hud_text += U16_TEXT("\nF8: Cycle BindSlotSummary");
        hud_text += U16_TEXT("\nF9: Toggle DescriptorDiag");
        hud_text += U16_TEXT("\nF10: Toggle MaterialQuery");

        ecs_debug_hud_text->SetText(hud_text);
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
