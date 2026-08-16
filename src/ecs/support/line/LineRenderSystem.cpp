#include <hgl/ecs/support/line/LineRenderSystem.h>
#include <hgl/ecs/support/line/LineBuildSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    LineRenderSystem::LineRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Line");
        AddDependency<LineBuildSystem>();
        AddDependency<RenderBufferUploadSystem>();
    }

    RenderPipelineBase* LineRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline(LineRenderPipeline::kName);
    }

    void LineRenderSystem::OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd)
    {
        if (!pipeline)
        {
            GLogWarning("[LineRenderSystem] OnRender skipped: pipeline is null");
            return;
        }

        if (!cmd)
        {
            GLogWarning("[LineRenderSystem] OnRender skipped: cmd buffer is null");
            return;
        }

        auto* line_pipeline = dynamic_cast<LineRenderPipeline*>(pipeline);
        if (line_pipeline)
        {
            GLogInfo("[LineRenderSystem] Render begin: pipeline=%p cmd=%p total_lines=%u",
                     pipeline,
                     cmd,
                     line_pipeline->GetTotalLineCount());
        }

        pipeline->Render(cmd);

        if (line_pipeline)
        {
            GLogInfo("[LineRenderSystem] Render end: pipeline=%p cmd=%p total_lines=%u",
                     pipeline,
                     cmd,
                     line_pipeline->GetTotalLineCount());
        }
    }

}  // namespace hgl::ecs
