#include <hgl/ecs/support/primitive/PrimitiveBuildSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    PrimitiveBuildSystem::PrimitiveBuildSystem(const std::string& name)
        : BuildSystem(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveBuildSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveBuildSystem::OnBuild(RenderPipelineBase* pipeline)
    {
        pipeline->RunBuild();
    }
}
