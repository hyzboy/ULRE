#include<hgl/ecs/RenderPrimitiveRenderSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/RenderPrimitiveSystem.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<iostream>

namespace hgl::ecs
{
    RenderPrimitiveRenderSystem::RenderPrimitiveRenderSystem(const std::string& name)
        : System(name)
    {
    }

    void RenderPrimitiveRenderSystem::Update(float /*deltaTime*/)
    {
    }

    void RenderPrimitiveRenderSystem::Render(graph::RenderCmdBuffer* cmdBuffer, float /*deltaTime*/)
    {
        if (!world || !cmdBuffer)
            return;

        auto batch_system = world->GetSystem<RenderPrimitiveSystem>();
        if (!batch_system)
        {
            if (!warned_missing_batch_system)
            {
                std::cout << "[RenderPrimitiveRenderSystem::Render] WARNING: Missing RenderPrimitiveSystem" << std::endl;
                warned_missing_batch_system = true;
            }
            return;
        }

        batch_system->RenderPrimitives(cmdBuffer);
    }
}//namespace hgl::ecs
