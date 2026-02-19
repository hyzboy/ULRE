#pragma once

#include <vector>

namespace hgl { class Color4f; }

namespace hgl::ecs { class ECSContext; }

namespace hgl::graph
{
    class RenderCmdBuffer;
    class IRenderTarget;
    class RenderTask;
    class LineRenderManager;

    struct RenderStageContext
    {
        RenderCmdBuffer *   cmd                 = nullptr;
        IRenderTarget *     render_target       = nullptr;
        RenderTask *        render_task         = nullptr;
        ecs::ECSContext *   ecs_context         = nullptr;
        LineRenderManager * line_render_manager = nullptr;
        const hgl::Color4f *clear_color         = nullptr;
        bool                render_result       = false;
    };

    class RenderStage
    {
    public:

        virtual ~RenderStage() = default;

        virtual const char *GetName() const { return ""; }
        virtual bool IsEnabled() const { return true; }
        virtual void Prepare(RenderStageContext &) {}
        virtual void Execute(RenderStageContext &) = 0;
    };

    class RenderStagePipeline
    {
        std::vector<RenderStage *> stages;

    public:

        void AddStage(RenderStage *stage);
        bool RemoveStage(RenderStage *stage);
        bool RemoveStage(const char *stage_name);
        void ClearStages();

        bool InsertStage(RenderStage *stage,size_t index);
        bool InsertStageBefore(RenderStage *stage,RenderStage *before_stage);
        bool InsertStageAfter(RenderStage *stage,RenderStage *after_stage);
        bool InsertStageBefore(RenderStage *stage,const char *before_name);
        bool InsertStageAfter(RenderStage *stage,const char *after_name);
        bool MoveStage(RenderStage *stage,size_t new_index);
        bool MoveStage(const char *stage_name,size_t new_index);

        bool ReplaceStage(const char *stage_name,RenderStage *new_stage);

        int GetStageIndex(RenderStage *stage) const;
        int GetStageIndex(const char *stage_name) const;
        size_t GetStageCount() const { return stages.size(); }

        const std::vector<RenderStage *> &GetStages() const { return stages; }

        void Execute(RenderStageContext &context);
    };
}//namespace hgl::graph
