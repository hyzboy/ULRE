#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
        class IRenderTarget;
        class RenderContext;
        class LineRenderService;
    }

    namespace ecs
    {
        class LineRenderSystem : public System
        {
        private:
            graph::LineRenderService *line_service = nullptr;
            graph::RenderContext *render_context = nullptr;
            graph::IRenderTarget *render_target = nullptr;

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override;

            void SetRenderContext(graph::RenderContext *ctx);
            void SetRenderTarget(graph::IRenderTarget *rt);

            void SetLineRenderService(graph::LineRenderService *svc);
            graph::LineRenderService *GetLineRenderService() const { return line_service; }

            void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        };
    }//namespace ecs
}//namespace hgl

