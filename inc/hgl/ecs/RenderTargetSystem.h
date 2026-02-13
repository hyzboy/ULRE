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
         * Keeps render-target related references in sync for ECS systems.
         */
        class RenderTargetSystem : public System
        {
        private:

            graph::RenderFramework *render_framework = nullptr;
            graph::IRenderTarget *render_target = nullptr;

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

            void SyncSubsystems();
        };
    }//namespace ecs
}//namespace hgl
