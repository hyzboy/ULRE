#include <hgl/ecs/support/line/LineRenderSystem.h>
#include <hgl/ecs/support/line/LineBuildSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    LineRenderSystem::LineRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderDrawSubmit);
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
        pipeline->Render(cmd);
    }

}  // namespace hgl::ecs
