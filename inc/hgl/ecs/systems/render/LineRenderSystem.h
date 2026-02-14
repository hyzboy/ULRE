#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
        class IRenderTarget;
        class RenderContext;
        class LineRenderManager;
    }

    namespace ecs
    {
        class LineRenderSystem : public System
        {
        private:

            graph::LineRenderManager *line_manager = nullptr;
            graph::RenderContext *render_context = nullptr;
            graph::IRenderTarget *render_target = nullptr;

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }
            void SetRenderTarget(graph::IRenderTarget *rt);

            void SetLineRenderManager(graph::LineRenderManager *mgr) { line_manager = mgr; }
            graph::LineRenderManager *GetLineRenderManager() const { return line_manager; }

            void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        private:

            void EnsureLineManager();
        };
    }//namespace ecs
}//namespace hgl

