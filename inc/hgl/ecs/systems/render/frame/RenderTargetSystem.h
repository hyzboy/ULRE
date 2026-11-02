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

            /// [框架内部接线] 由 ECSContext::OnResize 等场景调用，用于向子系统同步 RT 引用
            void SetRenderTarget(graph::IRenderTarget *rt);

            /// [系统内缓存] RenderTargetSystem 自身缓存的 RT 引用。
            /// 应用代码请用 ecs::ECSContext::GetRenderTarget()（唯一权威 getter）。
            graph::IRenderTarget *GetRenderTarget() const { return render_target; }

            void Update(float deltaTime) override;

        private:

            void SyncSubsystems();
        };
    }//namespace ecs
}//namespace hgl

