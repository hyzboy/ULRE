#pragma once

#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include <hgl/graph/core/GraphicsContext.h>
namespace hgl::graph
{
    namespace mtl
    {
        class ShaderBuildContext;
    }

    /**
     * RenderContext: 渲染执行上下文
     *
     * 职责（收敛后）:
     * - 仅作为"当前 GraphicsContext 的轻桥"，供持有它的系统/管线取资源管理器
     *
     * 曾经持有的 current_render_target / current_render_cmd_buf 两份状态
     * 已删除——它们与 ECSContext::render_target / current_render_cmd 完全
     * 重复，靠每帧同步维持一致，不同步即是跨 Pass 管线误用 bug 的根源
     * （见 c778f1fb2）。
     *
     * 当前渲染目标唯一权威：ecs::ECSContext::GetRenderTarget()
     * 当前命令缓冲唯一权威：ecs::ECSContext::GetCurrentRenderCmd()
     */
    class RenderContext
    {
    private:
        GraphicsContext* graphics_context = nullptr;

    public:
        RenderContext() = default;

        virtual ~RenderContext() = default;

        // 禁用复制构造和赋值
        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;

    public:

        void SetGraphicsContext(GraphicsContext* ctx) { graphics_context = ctx; }
        GraphicsContext* GetGraphicsContext() const { return graphics_context; }

        template<typename T> T *GetManager()
        {
            return graphics_context?graphics_context->GetManager<T>():nullptr;
        }
    }; // class RenderContext
} // namespace hgl::graph
