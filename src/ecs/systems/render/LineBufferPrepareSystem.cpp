#include<hgl/ecs/systems/render/LineBufferPrepareSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    LineBufferPrepareSystem::LineBufferPrepareSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame);
    }

    void LineBufferPrepareSystem::Update(float /*deltaTime*/)
    {
        ECSContext *ctx = world ? world : context;
        if (!ctx)
            return;

        auto line_render_system = ctx->GetSystem<LineRenderSystem>();
        if (!line_render_system)
            return;

        line_render_system->PrepareBuffersForCurrentFrame();
    }
}//namespace hgl::ecs
