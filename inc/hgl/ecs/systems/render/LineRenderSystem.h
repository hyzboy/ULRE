#pragma once

#include<hgl/ecs/core/System.h>
#include<cstdint>

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
         * CN: 线条渲染系统
         *
         * Update() 运行于 RenderBatch 阶段：
         *   重置批次，遍历可见 LinesComponent，将数据写入 StagedBuffer VAB（自动标脏）。
         *   RenderBufferUploadSystem 在 RenderBufferUpload 阶段自动上传所有脏缓冲。
         *
         * Render() 运行于 RenderDrawSubmit 阶段，仅录制绘制命令。
         *
         * EN: Line Render System
         *
         * Update() runs in RenderBatch phase:
         *   Resets batches, iterates visible LinesComponents, writes into StagedBuffer VABs (auto-dirty).
         *   RenderBufferUploadSystem auto-uploads all dirty buffers in RenderBufferUpload phase.
         *
         * Render() runs in RenderDrawSubmit phase: records draw commands only.
         */
        class LineRenderSystem : public System
        {
        private:
            graph::LineRenderManager *line_manager      = nullptr;
            bool                      own_line_manager    = false;
            bool                      manager_initialized = false;
            uint32_t                  last_batch_frame_   = UINT32_MAX; ///< frame-level guard: skip re-batch in same frame

        public:

            LineRenderSystem(const std::string &name = "LineRenderSystem");
            ~LineRenderSystem() override;

            void Initialize() override;
            void Shutdown()   override;

            /// RenderBatch: ClearLines + write visible component lines → StagedBuffer VABs
            void Update(float deltaTime) override;

            /// RenderDrawSubmit: issue draw commands only (VABs already uploaded)
            void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

            void SetLineRenderManager(graph::LineRenderManager *mgr, bool take_ownership = true)
            {
                if (line_manager == mgr) { own_line_manager = take_ownership; return; }
                if (line_manager && own_line_manager) delete line_manager;
                line_manager     = mgr;
                own_line_manager = take_ownership;
            }

            graph::LineRenderManager *GetLineRenderManager() const { return line_manager; }

            uint32_t GetLineCount() const;

            /// Set palette color (deferred until manager is initialized)
            void SetColor(int index, const hgl::Color4f &color);
        };
    }//namespace ecs
}//namespace hgl

