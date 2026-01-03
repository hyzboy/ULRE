#pragma once

#include<hgl/graph/RenderTask.h>
#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/World.h>
#include<hgl/type/Map.h>
#include<hgl/graph/RenderContext.h>
// ECS forward declaration
namespace hgl::ecs { class ECSContext; }

namespace hgl::graph
{
    class World;
    class RenderContext;    // forward
    class RenderCmdBuffer;  // forward
    class Pipeline;         // fwd for CreatePipeline
    class Material;         // fwd for CreatePipeline
    class SceneNode;        // fwd for GetWorldRootNode
    enum class InlinePipeline; // fwd for CreatePipeline

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

        // CameraControl 始终由 SceneRenderer 负责创建/管理/释放
        CameraControl * camera_control_owned = nullptr;

    protected:

        Color4f clear_color;                       ///< 清屏颜色
        bool    render_state_dirty=false;

    public:

                RenderPass *        GetRenderPass       ()      {return render_target->GetRenderPass();}
        const   ViewportInfo *      GetViewportInfo     ()const {return render_context?render_context->GetViewportInfo():nullptr;}
        const   VkExtent2D &        GetExtent           ()const {return render_context->GetExtent();}

                World *             GetWorld            ()const {return render_context?render_context->GetWorld():nullptr;}
                ecs::ECSContext *   GetECSContext       ()const {return render_context?render_context->GetECSContext():nullptr;}
                Camera *            GetCamera           ()const {return render_context?render_context->GetCamera():nullptr;}
        const   CameraInfo *        GetCameraInfo       ()const {return render_context?render_context->GetCameraInfo():nullptr;}
                CameraControl *     GetCameraControl    ()const {return camera_control_owned;}
                LineRenderManager * GetLineRenderManager()const {return render_context?render_context->GetLineRenderManager():nullptr;}
                RenderContext *     GetRenderContext    ()const override {return render_context;}

                // 便捷方法：取得场景根节点
                SceneNode *         GetWorldRootNode        ()const {auto s=GetWorld(); return s?s->GetRootNode():nullptr;}
                // 便捷方法：基于当前RenderPass创建内置管线
                Pipeline *          CreatePipeline      (Material *mtl,const InlinePipeline &ip)
                { return GetRenderPass()?GetRenderPass()->CreatePipeline(mtl,ip):nullptr; }
                // 便捷方法：等待当前渲染队列空闲
                void                WaitQueue           (){ if(render_target) render_target->WaitQueue(); }
                // 便捷方法：渲染->提交->等待
                bool                RenderSubmitAndWait ()
                {
                    if(!RenderFrame()) return false;
                    if(!Submit()) return false;
                    WaitQueue();
                    return true;
                }

    public:

        SceneRenderer(RenderFramework *,IRenderTarget *);
        virtual ~SceneRenderer();

        bool SetRenderTarget(IRenderTarget *);
        void SetWorld(World *);
        void SetECSContext(ecs::ECSContext *ctx){ if(render_context) render_context->SetECSContext(ctx); }
        void SetCameraControl(CameraControl *);     ///< 设定新的相机控制器（SceneRenderer接管并负责释放）
        void UseDefaultCameraControl();             ///< 使用默认第一人称相机控制器

        void SetClearColor(const Color4f &c){clear_color=c;}

        void Tick(double);

        bool BeginRender();
        bool RenderFrame();
        bool Submit();
    };//class SceneRenderer
}//namespace hgl::graph
