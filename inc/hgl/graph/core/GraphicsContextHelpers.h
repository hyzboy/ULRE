#pragma once

#include <memory>

namespace hgl::ecs {
    class ECSContext;
}

namespace hgl::graph
{
    class RenderFramework;
    class IGraphicsContext;

    std::shared_ptr<IGraphicsContext> CreateGraphicsContextFromRenderFramework(RenderFramework* rf);
    bool AttachGraphicsContext(hgl::ecs::ECSContext* ecs_ctx, RenderFramework* rf);
}
