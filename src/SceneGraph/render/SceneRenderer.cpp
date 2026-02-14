#include<hgl/graph/SceneRenderer.h>
#include<iostream>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/RenderStages.h>

namespace hgl::graph
{
    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_framework = rf;
        render_target = rt;
        clear_color.set(0,0,0,1);
    }

    SceneRenderer::~SceneRenderer()
    {
    }

    Camera *SceneRenderer::GetCamera() const
    {
        auto ecs_ctx = GetECSContext();
        if (!ecs_ctx)
            return nullptr;

        auto camera_system = ecs_ctx->GetSystem<ecs::CameraSystem>();
        return camera_system ? camera_system->GetCamera() : nullptr;
    }

    const ViewportInfo *SceneRenderer::GetViewportInfo() const
    {
        auto ecs_ctx = GetECSContext();
        if (ecs_ctx)
        {
            auto camera_system = ecs_ctx->GetSystem<ecs::CameraSystem>();
            if (camera_system)
                return camera_system->GetViewportInfo();
        }

        return render_target ? render_target->GetViewportInfo() : nullptr;
    }

    const CameraInfo *SceneRenderer::GetCameraInfo() const
    {
        auto ecs_ctx = GetECSContext();
        if (!ecs_ctx)
            return nullptr;

        auto camera_system = ecs_ctx->GetSystem<ecs::CameraSystem>();
        return camera_system ? camera_system->GetCameraInfo() : nullptr;
    }

    LineRenderManager *SceneRenderer::GetLineRenderManager() const
    {
        auto ecs_ctx = GetECSContext();
        if (!ecs_ctx)
            return nullptr;

        auto line_system = ecs_ctx->GetSystem<ecs::LineRenderSystem>();
        return line_system ? line_system->GetLineRenderManager() : nullptr;
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        //不要做render_target==rt测试，因为真的有机率旧的删掉后，再new出来新的地址一样

        render_target=rt;
        SyncRenderTargetSystem();

        return(true);
    }

    void SceneRenderer::SetECSContext(ecs::ECSContext *ctx)
    {
        ecs_context = ctx;
        SyncRenderTargetSystem();
    }

    void SceneRenderer::SyncRenderTargetSystem()
    {
        if (!ecs_context)
            return;

        auto render_target_system = ecs_context->GetSystem<ecs::RenderTargetSystem>();
        if (!render_target_system)
            return;

        render_target_system->SetRenderFramework(render_framework);
        render_target_system->SetRenderTarget(render_target);
    }

    void SceneRenderer::Tick(double delta)
    {
        if(GetECSContext())
        {
            GetECSContext()->Tick(static_cast<float>(delta));
            // 渲染系统在 RenderFrame 调用
        }
    }

    void SceneRenderer::EnsureEcsPipeline()
    {
        BuildEcsPipeline(ecs_pipeline);
    }

    bool SceneRenderer::RenderFrame()
    {
        // ECS 渲染路径：目前仅执行空渲染流程，便于后续接入 ECS RenderSystem
        if(!GetECSContext())
            return(false);

        if(!render_target)
            return(false);

        EnsureEcsPipeline();

        RenderStageContext ctx{};
        ctx.render_target = render_target;
        ctx.ecs_context = GetECSContext();
        ctx.clear_color = &clear_color;

        ecs_pipeline.Execute(ctx);

        render_state_dirty = true; // 标记有提交
        return true;
     }

     bool SceneRenderer::Submit()
     {
         if(!render_target||!render_state_dirty)
             return(false);

        bool ok = render_target->Submit();
        return ok;
     }
 }//namespace hgl::graph

