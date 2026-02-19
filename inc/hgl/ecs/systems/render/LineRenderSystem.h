#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    class Color4f;

    namespace graph
    {
        class RenderCmdBuffer;
        class LineRenderManager;
    }

    namespace ecs
    {
        /**
         * CN: 线条渲染系统 - 收集所有 LinesComponent 并渲染
         * EN: Line Render System - Collect all LinesComponent and render them
         */
        class LineRenderSystem : public System
        {
        private:
            graph::LineRenderManager *line_manager = nullptr;
            bool manager_initialized = false;

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override = default;

            void Initialize() override;

            /**
             * CN: 设置 LineRenderManager（外部创建并传入）
             * EN: Set LineRenderManager (created externally)
             */
            void SetLineRenderManager(graph::LineRenderManager *mgr) { line_manager = mgr; }

            graph::LineRenderManager *GetLineRenderManager() const { return line_manager; }

            /**
             * CN: 设置调色板中的颜色（会等待 Manager 初始化）
             * EN: Set palette color (will wait for Manager initialization)
             */
            void SetColor(int index, const hgl::Color4f &color);

            /**
             * CN: 每帧收集所有 LinesComponent 并同步到渲染器
             * EN: Per-frame collect all LinesComponent and sync to renderer
             */
            void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        private:

            /**
             * CN: 同步 LinesComponent 到 LineRenderManager
             * EN: Sync LinesComponent to LineRenderManager
             */
            void SyncComponentsToRenderer();
        };
    }//namespace ecs
}//namespace hgl

