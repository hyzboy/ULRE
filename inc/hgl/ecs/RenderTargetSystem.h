#pragma once

#include<hgl/ecs/System.h>

namespace hgl
{
    namespace graph
    {
        class RenderFramework;
        class IRenderTarget;
    }

    namespace ecs
    {
        /**
         * RenderTargetSystem
         *
         * Prepares render target data before render submission:
         * - sync viewport info and UBO
         * - update camera viewport binding
         * - propagate render target to dependent render systems
         */
        class RenderTargetSystem : public System
        {
        private:

            graph::RenderFramework *render_framework = nullptr;
            graph::IRenderTarget *render_target = nullptr;
            uint32_t last_width = 0;
            uint32_t last_height = 0;
            bool extent_valid = false;

        public:

            RenderTargetSystem(const std::string &name = "RenderTargetSystem");
            ~RenderTargetSystem() override = default;

        public:

            void SetRenderFramework(graph::RenderFramework *rf);
            void SetRenderTarget(graph::IRenderTarget *rt);

            graph::RenderFramework *GetRenderFramework() const { return render_framework; }
            graph::IRenderTarget *GetRenderTarget() const { return render_target; }

            void Update(float deltaTime) override;

        private:

            void SyncViewport();
            void SyncSubsystems();
        };
    }//namespace ecs
}//namespace hgl
