#pragma once

#include<hgl/ecs/System.h>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
        class RenderFramework;
        class IRenderTarget;
        class LineRenderManager;
    }

    namespace ecs
    {
        class LineRenderSystem : public System
        {
        private:

            graph::LineRenderManager *line_manager = nullptr;
            graph::RenderFramework *render_framework = nullptr;
            graph::IRenderTarget *render_target = nullptr;

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override;

            void SetRenderFramework(graph::RenderFramework *rf);
            void SetRenderTarget(graph::IRenderTarget *rt);

            void SetLineRenderManager(graph::LineRenderManager *mgr) { line_manager = mgr; }
            graph::LineRenderManager *GetLineRenderManager() const { return line_manager; }

            void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        private:

            void EnsureLineManager();
        };
    }//namespace ecs
}//namespace hgl
