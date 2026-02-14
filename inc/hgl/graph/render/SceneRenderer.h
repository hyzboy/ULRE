#pragma once

#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/render/RenderStagePipeline.h>
#include<hgl/io/event/EventDispatcher.h>
// ECS forward declaration
namespace hgl::ecs { class ECSContext; }

namespace hgl::graph
{
    class RenderCmdBuffer;  // forward
    class Pipeline;         // fwd for CreatePipeline
    class Material;         // fwd for CreatePipeline
    enum class InlinePipeline; // fwd for CreatePipeline
    class RenderFramework;
    class Camera;
    struct CameraInfo;

    class SceneEventDispatcher:public hgl::io::EventDispatcher
    {
    public:

        using hgl::io::EventDispatcher::EventDispatcher;
        virtual ~SceneEventDispatcher() = default;
    };

    /**
    * 场景渲染器
    */
    class SceneRenderer:public SceneEventDispatcher
    {
        RenderFramework * render_framework = nullptr;
        IRenderTarget * render_target = nullptr;   ///< 当前渲染目标(便捷缓存)
        ecs::ECSContext * ecs_context = nullptr;

        RenderStagePipeline ecs_pipeline;          ///< ECS 渲染阶段管线

    protected:

        Color4f clear_color;                       ///< 清屏颜色
        bool    render_state_dirty=false;

    public:

                RenderPass *        GetRenderPass       ()      {return render_target->GetRenderPass();}
        const   ViewportInfo *      GetViewportInfo     ()const;
        const   VkExtent2D &        GetExtent           ()const {return render_target->GetExtent();}

                ecs::ECSContext *   GetECSContext       ()const {return ecs_context;}
                Camera *            GetCamera           ()const;
        const   CameraInfo *        GetCameraInfo       ()const;
                LineRenderManager * GetLineRenderManager()const;

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
        void SetECSContext(ecs::ECSContext *ctx);
        void SetClearColor(const Color4f &c){clear_color=c;}

        void Tick(double);

        RenderStagePipeline &GetEcsPipeline(){ return ecs_pipeline; }
        const RenderStagePipeline &GetEcsPipeline()const{ return ecs_pipeline; }
        void EnsureEcsPipeline();

        bool RenderFrame();
        bool Submit();

    private:

        void SyncRenderTargetSystem();
    };//class SceneRenderer
}//namespace hgl::graph
