#pragma once

#include<hgl/ecs/core/System.h>
#include<unordered_set>

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
        class LinesComponent;

        /**
         * CN: 线条渲染系统 - 收集所有 LinesComponent 并渲染
         * EN: Line Render System - Collect all LinesComponent and render them
         */
        class LineRenderSystem : public System
        {
        private:
            graph::LineRenderManager *line_manager = nullptr;
            bool own_line_manager = false;
            bool manager_initialized = false;
            std::unordered_set<uint64_t> active_component_keys;
            bool has_uploaded_once = false;
            uint32_t last_uploaded_line_count = 0;
            uint64_t last_collect_visible_set_signature = 0;

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override;

            void Initialize() override;
            void Shutdown() override;

            /**
             * CN: 设置 LineRenderManager（外部创建并传入）
             * EN: Set LineRenderManager (created externally)
             */
            void SetLineRenderManager(graph::LineRenderManager *mgr, bool take_ownership = true)
            {
                if (line_manager == mgr)
                {
                    own_line_manager = take_ownership;
                    return;
                }

                if (line_manager && own_line_manager)
                    delete line_manager;

                line_manager = mgr;
                own_line_manager = take_ownership;
            }

            graph::LineRenderManager *GetLineRenderManager() const { return line_manager; }
            uint32_t GetLastUploadedLineCount() const { return last_uploaded_line_count; }

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
            uint64_t MakeComponentKey(const LinesComponent* comp) const;
        };
    }//namespace ecs
}//namespace hgl

