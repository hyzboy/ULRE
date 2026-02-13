#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/math/geometry/Ray.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

// ECS forward decl
namespace hgl::ecs { class ECSContext; }

namespace hgl::graph
{
    /** 渲染上下文，聚合与单帧渲染相关资源(可持续扩展) */
    class RenderContext
    {
                RenderFramework *   rf                  = nullptr;
                IRenderTarget *     render_target       = nullptr;  ///< 当前渲染目标
        const   ViewportInfo *      viewport_info       = nullptr;  ///< 缓存视口
                World *             world               = nullptr;  ///< 世界指针(不负责释放)
                ecs::ECSContext *   ecs_context         = nullptr;  ///< ECS 上下文（可选，不负责释放）
                LineRenderManager * line_render_mgr     = nullptr;  ///< 线段渲染管理器

    public:

        const   ViewportInfo *      GetViewportInfo     ()const { return viewport_info; }
        const   Vector2u &          GetViewportSize     ()const { return viewport_info->GetViewport(); }
        const   VkExtent2D &        GetExtent           ()const { return render_target->GetExtent(); }

                World *             GetWorld            ()const { return world; }
                ecs::ECSContext *   GetECSContext       ()const { return ecs_context; }
                LineRenderManager * GetLineRenderManager()const { return line_render_mgr; }

    public:

        RenderContext(RenderFramework *rf,IRenderTarget *rt);
        ~RenderContext();

        void SetRenderTarget(IRenderTarget *rt);
        void SetWorld(World *s){ world = s; }
        void SetECSContext(ecs::ECSContext *ctx);

    public:

        void Tick(double delta);

        void BindDescriptor(RenderCmdBuffer *cmd);   ///< 绑定描述符：摄像机 + 场景
    };//class RenderContext
}//namesapce hgl::graph
