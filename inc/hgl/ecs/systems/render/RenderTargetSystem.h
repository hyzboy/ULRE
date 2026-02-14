#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        class IRenderTarget;
        class RenderContext;
    }

    namespace ecs
    {
        /**
         * RenderTargetSystem
         *
         * Keeps render-target related references in sync for ECS systems.
         */
        class RenderTargetSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;
            graph::IRenderTarget *render_target = nullptr;

        public:

            RenderTargetSystem(const std::string &name = "RenderTargetSystem");
            ~RenderTargetSystem() override = default;

        public:

            void SetRenderContext(graph::RenderContext *ctx);
            void SetRenderTarget(graph::IRenderTarget *rt);
            graph::IRenderTarget *GetRenderTarget() const { return render_target; }

            void Update(float deltaTime) override;

        private:

            void SyncSubsystems();
        };
    }//namespace ecs
}//namespace hgl

