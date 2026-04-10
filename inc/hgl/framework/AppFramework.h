#pragma once

#include <hgl/platform/Window.h>
#include <hgl/vk/VKInstance.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/type/String.h>
#include <hgl/math/Vector.h>
#include <hgl/graph/module/SwapchainModule.h>
#include <hgl/ecs/core/DefaultSystems.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class GraphicsContext;
        class SwapchainModule;
        class SwapchainRenderTarget;
        class RenderContext;
        class FontSource;
    }

    namespace ecs
    {
        class ECSContext;
        class TextComponent;
    }

    /**
     * AppFramework - Application lifecycle management
     *
     * Responsibilities:
     * - Window creation and management
     * - Vulkan instance and device initialization
     * - Event dispatching
     * - Application lifecycle (Init, Tick, Resize, Close)
     *
     * Does NOT:
     * - Implement graphics context interface (use VulkanGraphicsContext)
     * - Directly manage graphics resources (delegated to VulkanGraphicsContext)
     *
     * Usage with WorkManager:
     * ```cpp
     * AppFramework app("MyApp");
     * app.Init(1280, 720);
     * WorkManager wm(&app);
    * auto world = std::shared_ptr<ecs::ECSContext>(app.GetECSContext(), [](ecs::ECSContext*){});
    * MyWorkObject *wo = new MyWorkObject(world);
    * wm.Run(wo);
     * ```
     */
    class AppFramework : public io::WindowEvent
    {
    private:
        OSString app_name;

        Window *win = nullptr;
        graph::VulkanInstance *inst = nullptr;
        graph::VulkanDevice *device = nullptr;

        graph::GraphicsContext *graphics_context = nullptr;
        graph::SwapchainModule *sc_module = nullptr;

        ecs::ECSContext *default_ecs_context = nullptr;
        ecs::DefaultEcsDebugConfig default_ecs_debug_config;
        std::unique_ptr<graph::RenderContext> render_context;
        bool bind_slot_summary_toggle_key_pressed = false;
        bool debug_hud_toggle_key_pressed = false;
        bool descriptor_diag_toggle_key_pressed = false;
        bool material_query_toggle_key_pressed = false;
        bool ecs_debug_hud_visible = false;
        graph::FontSource *ecs_debug_hud_font = nullptr;
        std::shared_ptr<ecs::TextComponent> ecs_debug_hud_text;

    protected:

        math::Vector2i mouse_coord;

        virtual io::EventProcResult OnEvent(const io::EventHeader &header, const uint64 data) override;

    public:
        explicit AppFramework(const OSString &name);
        virtual ~AppFramework();

        // Disable copy
        AppFramework(const AppFramework &) = delete;
        AppFramework &operator=(const AppFramework &) = delete;

    public:
        /**
         * Initialize application
         * Creates window, Vulkan device, graphics context, and default ECS context
         * @param w Window width
         * @param h Window height
         * @return true if successful, false otherwise
         */
        virtual bool Init(uint w, uint h);
        virtual bool Init(uint w, uint h, int argc, os_char **argv);

    public:
        // Event callbacks
        virtual void OnResize(uint w, uint h);
        virtual void OnActive(bool active);
        virtual void OnClose();
        virtual void Tick();

    public:
        // High-level ECS debug config entry for editor/UI integration
        void SetDefaultEcsDebugConfig(const ecs::DefaultEcsDebugConfig &config);
        const ecs::DefaultEcsDebugConfig &GetDefaultEcsDebugConfig() const { return default_ecs_debug_config; }
        void SetBindSlotSummaryLogMode(ecs::BindSlotSummaryLogMode mode);
        ecs::BindSlotSummaryLogMode GetBindSlotSummaryLogMode() const { return default_ecs_debug_config.bind_slot_summary_log_mode; }
        void CycleBindSlotSummaryLogMode();
        void SetDescriptorContractDiagLogEnabled(bool enabled);
        bool IsDescriptorContractDiagLogEnabled() const { return default_ecs_debug_config.descriptor_contract_diag_log_enabled; }
        void ToggleDescriptorContractDiagLogEnabled();
        void SetMaterialBindingQueryLogEnabled(bool enabled);
        bool IsMaterialBindingQueryLogEnabled() const { return default_ecs_debug_config.material_binding_query_log_enabled; }
        void ToggleMaterialBindingQueryLogEnabled();
        void SetEcsDebugHudVisible(bool visible);
        bool IsEcsDebugHudVisible() const { return ecs_debug_hud_visible; }

    private:
        void InitializeEcsDebugHud();
        void UpdateEcsDebugHudText();

    public:
        // Application state access
        const OSString &GetAppName() const { return app_name; }
        Window *GetWindow() const { return win; }
        graph::VulkanInstance *GetInstance() const { return inst; }
        graph::VulkanDevice *GetDevice() const { return device; }
        const math::Vector2i &GetMouseCoord() const { return mouse_coord; }

        // Graphics context access
        graph::GraphicsContext *GetGraphicsContext() { return graphics_context; }
        const graph::GraphicsContext *GetGraphicsContext() const { return graphics_context; }

        // Swapchain access
        graph::SwapchainModule *GetSwapchainModule() { return sc_module; }
        graph::SwapchainRenderTarget *GetSwapchainRenderTarget()
        {
            return sc_module ? sc_module->GetRenderTarget() : nullptr;
        }
        const graph::SwapchainRenderTarget *GetSwapchainRenderTarget() const
        {
            return sc_module ? sc_module->GetRenderTarget() : nullptr;
        }

        // ECS access
        ecs::ECSContext *GetECSContext() { return default_ecs_context; }
        const ecs::ECSContext *GetECSContext() const { return default_ecs_context; }

        graph::RenderContext *GetRenderContext() { return render_context.get(); }
        const graph::RenderContext *GetRenderContext() const { return render_context.get(); }
    };

} // namespace hgl
