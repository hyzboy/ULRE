#include<hgl/ecs/RenderPrimitiveSubmitSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/MaterialBatch.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<iostream>

namespace hgl::ecs
{
    RenderPrimitiveSubmitSystem::RenderPrimitiveSubmitSystem(const std::string& name)
        : System(name)
    {
    }

    void RenderPrimitiveSubmitSystem::Render(graph::RenderCmdBuffer* cmdBuffer, float /*deltaTime*/)
    {
        if (!world || !cmdBuffer)
            return;

        auto& cache = world->GetRenderFrameCache();

        if (cache.renderableCount == 0)
        {
            std::cerr << "[RenderPrimitiveSubmitSystem::Render] WARNING: No renderable items!" << std::endl;
            return;
        }

        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (batch && batch->GetCount() > 0)
                batch->Render(cmdBuffer);
        }
    }
}//namespace hgl::ecs
