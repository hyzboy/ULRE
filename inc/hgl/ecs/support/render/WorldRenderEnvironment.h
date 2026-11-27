#pragma once

#include <hgl/graph/ubo/EnvironmentInfo.h>
#include <hgl/type/String.h>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class IRenderTarget;
        class GraphicsContext;
        class RenderContext;
        class EnvironmentManager;
    }

    namespace ecs
    {
        class WorldRenderEnvironment
        {
        private:
            hgl::graph::VulkanDevice *gpu_device = nullptr;
            hgl::graph::IRenderTarget *render_target = nullptr;
            hgl::graph::GraphicsContext *graphics_context = nullptr;
            hgl::graph::RenderContext *render_context = nullptr;

            hgl::graph::EnvProfileID env_profile = hgl::graph::kEnvProfileDefault;
            bool env_profile_explicit = false;
            bool env_profile_owned = false;

        public:
            hgl::graph::VulkanDevice *GetGPUDevice() const { return gpu_device; }
            void SetGPUDevice(hgl::graph::VulkanDevice *device) { gpu_device = device; }

            hgl::graph::IRenderTarget *GetRenderTarget() const { return render_target; }
            void SetRenderTarget(hgl::graph::IRenderTarget *target) { render_target = target; }

            hgl::graph::GraphicsContext *GetGraphicsContext() const { return graphics_context; }
            void SetGraphicsContext(hgl::graph::GraphicsContext *context) { graphics_context = context; }

            hgl::graph::RenderContext *GetRenderContext() const { return render_context; }
            void SetRenderContext(hgl::graph::RenderContext *context) { render_context = context; }

            hgl::graph::EnvProfileID GetEnvProfileID() const;
            hgl::graph::EnvProfileID GetExplicitEnvProfileID() const { return env_profile; }
            bool IsEnvProfileExplicit() const { return env_profile_explicit; }

            void ResetEnvProfile();
            void SetEnvProfile(hgl::graph::EnvProfileID id, bool take_ownership);
            bool IsEnvProfileOwned() const { return env_profile_owned; }
        };
    }
}
