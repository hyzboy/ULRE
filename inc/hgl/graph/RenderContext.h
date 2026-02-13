#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/math/geometry/Ray.h>

// ECS forward decl
namespace hgl::ecs { class ECSContext; }

namespace hgl::graph
{
    /** 渲染上下文，聚合与单帧渲染相关资源(可持续扩展) */
    class RenderContext
    {
                RenderFramework *   rf                  = nullptr;
                IRenderTarget *     render_target       = nullptr;  ///< 当前渲染目标
                ecs::ECSContext *   ecs_context         = nullptr;  ///< ECS 上下文（可选，不负责释放）

    public:

        const   VkExtent2D &        GetExtent           ()const { return render_target->GetExtent(); }

                ecs::ECSContext *   GetECSContext       ()const { return ecs_context; }

    public:

        RenderContext(RenderFramework *rf,IRenderTarget *rt);
        ~RenderContext();

        void SetRenderTarget(IRenderTarget *rt);
        void SetECSContext(ecs::ECSContext *ctx);

    public:

        void Tick(double delta);

        // Descriptor binding handled by render stages
    };//class RenderContext
}//namesapce hgl::graph
