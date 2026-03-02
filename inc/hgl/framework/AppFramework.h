#pragma once

#include <hgl/platform/Window.h>
#include <hgl/vk/VKInstance.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/type/String.h>
#include <hgl/math/Vector.h>
#include <hgl/graph/module/SwapchainModule.h>
#include <hgl/graph/module/ShaderGenPathMode.h>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class GraphicsContext;
        class SwapchainModule;
        class SwapchainRenderTarget;
        class RenderContext;
    }

    namespace ecs
    {
        class ECSContext;
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

        graph::ShaderGenPathMode shadergen_path_mode = graph::ShaderGenPathMode::MirrorValidate;
        bool shadergen_path_mode_overridden = false;

        ecs::ECSContext *default_ecs_context = nullptr;
        std::unique_ptr<graph::RenderContext> render_context;

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
        // Application state access
        const OSString &GetAppName() const { return app_name; }
        Window *GetWindow() const { return win; }
        graph::VulkanInstance *GetInstance() const { return inst; }
        graph::VulkanDevice *GetDevice() const { return device; }
        const math::Vector2i &GetMouseCoord() const { return mouse_coord; }

        // Graphics context access
        graph::GraphicsContext *GetGraphicsContext() { return graphics_context; }
        const graph::GraphicsContext *GetGraphicsContext() const { return graphics_context; }

        void SetShaderGenPathMode(const graph::ShaderGenPathMode mode)
        {
            shadergen_path_mode = mode;
            shadergen_path_mode_overridden = true;
        }
        void SetShaderGenPathModeName(const char *mode_name)
        {
            SetShaderGenPathMode(graph::ParseShaderGenPathMode(mode_name));
        }
        graph::ShaderGenPathMode GetShaderGenPathMode() const { return shadergen_path_mode; }

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
