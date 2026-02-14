#include <hgl/graph/core/GraphicsContextHelpers.h>
#include <hgl/graph/core/GraphicsModule.h>
#include <hgl/graph/render/RenderFramework.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::graph
{
    std::shared_ptr<IGraphicsContext> CreateGraphicsContextFromRenderFramework(RenderFramework* rf)
    {
        if (!rf)
            return nullptr;

        auto module = std::make_shared<GraphicsModule>(rf->GetDevice(),
                                                       rf->GetRenderPassManager(),
                                                       rf->GetTextureManager(),
                                                       rf->GetMaterialManager(),
                                                       rf->GetBufferManager(),
                                                       rf->GetSamplerManager(),
                                                       rf->GetGeometryManager(),
                                                       rf->GetPrimitiveManager());

        module->SetLegacyRenderFramework(rf);
        module->SetDefaultRenderPass(rf->GetDefaultRenderPass());

        return module;
    }

    bool AttachGraphicsContext(hgl::ecs::ECSContext* ecs_ctx, RenderFramework* rf)
    {
        if (!ecs_ctx)
            return false;

        auto ctx = CreateGraphicsContextFromRenderFramework(rf);
        if (!ctx)
            return false;

        ecs_ctx->SetGraphicsContext(ctx);
        return true;
    }
}
