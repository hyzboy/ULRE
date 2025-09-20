#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/Scene.h>
#include<hgl/type/Map.h>
#include<hgl/graph/RenderContext.h>

namespace hgl::graph
{
    class Scene;
    class RenderContext;    // forward
    class RenderCmdBuffer;  // forward

    using RenderTaskNameMap=Map<RenderTaskName,RenderTask *>;

    class SceneEventDispatcher:public io::EventDispatcher
    {
    public:

        using io::EventDispatcher::EventDispatcher;
        virtual ~SceneEventDispatcher() = default;

    public:

        virtual RenderContext *GetRenderContext()const=0;
    };

    /**
    * 场景渲染器
    */
    class SceneRenderer:public SceneEventDispatcher
    {
        IRenderTarget * render_target = nullptr;   ///< 当前渲染目标(便捷缓存)
        RenderContext * render_context = nullptr;  ///< 渲染上下文

        RenderTask *    render_task = nullptr;     ///< 当前渲染任务

    protected:

        Color4f clear_color;                       ///< 清屏颜色
        bool    render_state_dirty=false;

    public:

                RenderPass *        GetRenderPass       ()      {return render_target->GetRenderPass();}
        const   ViewportInfo *      GetViewportInfo     ()const {return render_context?render_context->GetViewportInfo():nullptr;}
        const   VkExtent2D &        GetExtent           ()const {return render_context->GetExtent();}

                Scene *             GetScene            ()const {return render_context?render_context->GetScene():nullptr;}
                Camera *            GetCamera           ()const {return render_context?render_context->GetCamera():nullptr;}
        const   CameraInfo *        GetCameraInfo       ()const {return render_context?render_context->GetCameraInfo():nullptr;}
                CameraControl *     GetCameraControl    ()const {return render_context?render_context->GetCameraControl():nullptr;}
                LineRenderManager * GetLineRenderManager()const {return render_context?render_context->GetLineRenderManager():nullptr;}

                RenderContext *     GetRenderContext    ()const override {return render_context;}

    public:

        SceneRenderer(RenderFramework *,IRenderTarget *);
        virtual ~SceneRenderer();

        bool SetRenderTarget(IRenderTarget *);
        void SetScene(Scene *);
        void SetCameraControl(CameraControl *);

        void SetClearColor(const Color4f &c){clear_color=c;}

        void Tick(double);

        bool BeginRender();
        bool RenderFrame();
        bool Submit();
    };//class SceneRenderer
}//namespace hgl::graph
