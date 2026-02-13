#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/World.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/graph/RenderContext.h>
#include<hgl/graph/RenderStagePipeline.h>
// ECS forward declaration
namespace hgl::ecs { class ECSContext; }

namespace hgl::graph
{
    enum class RenderPath
    {
        Ecs,
        Scene
    };

    class World;
    class RenderContext;    // forward
    class RenderCmdBuffer;  // forward
    class Pipeline;         // fwd for CreatePipeline
    class Material;         // fwd for CreatePipeline
    class SceneNode;        // fwd for GetWorldRootNode
    enum class InlinePipeline; // fwd for CreatePipeline
    class Camera;
    struct CameraInfo;

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

        RenderStagePipeline ecs_pipeline;          ///< ECS 渲染阶段管线(逐步迁移)

    protected:

        Color4f clear_color;                       ///< 清屏颜色
        bool    render_state_dirty=false;

    public:

                RenderPass *        GetRenderPass       ()      {return render_target->GetRenderPass();}
        const   ViewportInfo *      GetViewportInfo     ()const {return render_context?render_context->GetViewportInfo():nullptr;}
        const   VkExtent2D &        GetExtent           ()const {return render_context->GetExtent();}

                World *             GetWorld            ()const {return render_context?render_context->GetWorld():nullptr;}
                ecs::ECSContext *   GetECSContext       ()const {return render_context?render_context->GetECSContext():nullptr;}
                Camera *            GetCamera           ()const;
            const   CameraInfo *        GetCameraInfo       ()const;
                LineRenderManager * GetLineRenderManager()const;
                RenderContext *     GetRenderContext    ()const override {return render_context;}

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
        void SetClearColor(const Color4f &c){clear_color=c;}

        void Tick(double);

        RenderStagePipeline &GetEcsPipeline(){ return ecs_pipeline; }
        const RenderStagePipeline &GetEcsPipeline()const{ return ecs_pipeline; }
        void EnsureEcsPipeline();

        bool BeginRender();
        bool RenderFrame();
        bool Submit();
    };//class SceneRenderer
}//namespace hgl::graph
