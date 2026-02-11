#pragma once

#include<hgl/ecs/System.h>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
        class LineRenderManager;
    }

    namespace ecs
    {
        class LineRenderSystem : public System
        {
        private:

            graph::LineRenderManager *line_manager = nullptr;

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override = default;

            void SetLineRenderManager(graph::LineRenderManager *mgr) { line_manager = mgr; }
            graph::LineRenderManager *GetLineRenderManager() const { return line_manager; }

            void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;
        };
    }//namespace ecs
}//namespace hgl
