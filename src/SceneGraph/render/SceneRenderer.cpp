#include<hgl/graph/SceneRenderer.h>
#include<iostream>
#include<hgl/ecs/Context.h>
#include<hgl/graph/World.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<hgl/graph/camera/FirstPersonCameraControl.h>

namespace hgl::graph
{
    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_target=rt;
        render_context=new RenderContext(rf,rt);
        render_task=new RenderTask("DefaultRenderTask",rt);
        clear_color.set(0,0,0,1);

        // Ensure there is always a camera control managed by this renderer
        UseDefaultCameraControl();
    }

    SceneRenderer::~SceneRenderer()
    {
        // detach camera control from dispatcher chain
        if(camera_control_owned)
            RemoveChildDispatcher(camera_control_owned);

        // release owned camera
        SAFE_CLEAR(camera_control_owned);

        SAFE_CLEAR(render_task);
        SAFE_CLEAR(render_context);
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        //不要做render_target==rt测试，因为真的有机率旧的删掉后，再new出来新的地址一样

        render_target=rt;

        if(render_context)
            render_context->SetRenderTarget(rt);

        render_task->SetRenderTarget(rt);
        return(true);
    }

    void SceneRenderer::SetWorld(World *sw)
    {
        if(!render_context)return;

        World *old_scene = render_context->GetWorld();

        if(old_scene)
        {
            RemoveChildDispatcher(old_scene->GetEventDispatcher());
        }

        render_context->SetWorld(sw);

        AddChildDispatcher(sw->GetEventDispatcher());
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        // remove previous camera control from dispatcher chain
        if(camera_control_owned)
            RemoveChildDispatcher(camera_control_owned);

        // release previous camera control
        SAFE_CLEAR(camera_control_owned);

        camera_control_owned = cc;

        if(render_context)
            render_context->SetCameraControl(camera_control_owned);

        if(camera_control_owned)
        {
            render_task->SetCameraInfo(GetCameraInfo());
            AddChildDispatcher(camera_control_owned);
        }
    }

    void SceneRenderer::UseDefaultCameraControl()
    {
        auto *fpcc = new FirstPersonCameraControl();

        // attach keyboard/mouse helpers to the camera control so it can receive input
        auto *ckc = new CameraKeyboardControl(fpcc);
        auto *cmc = new CameraMouseControl(fpcc);
        fpcc->AddChildDispatcher(ckc);
        fpcc->AddChildDispatcher(cmc);

        SetCameraControl(fpcc); // renderer owns it by default now
    }

    void SceneRenderer::Tick(double delta)
    {
        if(render_context)
            render_context->Tick(delta);

        if(GetWorld())
            GetWorld()->Tick(delta);
        else if(GetECSContext())
        {
            GetECSContext()->Tick(static_cast<float>(delta));
            // 渲染系统在 RenderFrame 调用
        }
    }
    
    bool SceneRenderer::RenderFrame()
    {
        // ECS 渲染路径：目前仅执行空渲染流程，便于后续接入 ECS RenderSystem
        if(GetECSContext())
        {
            std::cerr << "[SceneRenderer] RenderFrame: ECS path, target=" << render_target << " ecs=" << GetECSContext() << std::endl;
            LineRenderManager *lrm=GetLineRenderManager();

            RenderCmdBuffer *cmd = render_target->BeginRender();
            std::cerr << "[SceneRenderer] BeginRender cmd=" << cmd << std::endl;

            if(render_context)
            {
                render_context->BindDescriptor(cmd);
                std::cerr << "[SceneRenderer] BindDescriptor done" << std::endl;
            }

            cmd->SetClearColor(0,clear_color);
            cmd->BeginRenderPass();
            std::cerr << "[SceneRenderer] BeginRenderPass" << std::endl;

            // TODO: 在此处接入 ECS 的渲染收集与绘制
            GetECSContext()->Render(cmd, 0.0f);
            std::cerr << "[SceneRenderer] ECSContext Render finished" << std::endl;

            if(lrm)
            {
                lrm->Draw(cmd);
                std::cerr << "[SceneRenderer] LineRenderManager Draw" << std::endl;
            }

            cmd->EndRenderPass();
            render_target->EndRender();
            std::cerr << "[SceneRenderer] EndRenderPass/EndRender" << std::endl;

            render_state_dirty = true; // 标记有提交
            return true;
        }

        // 旧 SceneGraph 渲染路径
        if(!GetWorld())
            return(false);

        SceneNode *root = GetWorld()->GetRootNode();
        if(!root)
            return(false);
        std::cerr << "[SceneRenderer] RenderFrame: SceneGraph path root=" << root << std::endl;

        root->UpdateWorldTransform();
        const uint renderable = render_task->RebuildRenderList(root);
        std::cerr << "[SceneRenderer] RebuildRenderList renderable=" << renderable << std::endl;

        LineRenderManager *lrm=GetLineRenderManager();
 
        if(renderable == 0 && (lrm&&lrm->GetLineCount()==0))
        {
            // nothing to draw this frame
            render_state_dirty = false;
            std::cerr << "[SceneRenderer] Nothing to draw" << std::endl;
            return true;    // treat as successful no-op
        }

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();
        std::cerr << "[SceneRenderer] BeginRender cmd=" << cmd << std::endl;

        render_context->BindDescriptor(cmd);
        std::cerr << "[SceneRenderer] BindDescriptor done" << std::endl;

        cmd->SetClearColor(0,clear_color);
        cmd->BeginRenderPass();
        std::cerr << "[SceneRenderer] BeginRenderPass" << std::endl;

        result=render_task->Render(cmd);
        std::cerr << "[SceneRenderer] RenderTask result=" << result << std::endl;

        if(lrm)
        {
            lrm->Draw(cmd);
            std::cerr << "[SceneRenderer] LineRenderManager Draw" << std::endl;
        }

        cmd->EndRenderPass();
        render_target->EndRender();
        std::cerr << "[SceneRenderer] EndRenderPass/EndRender" << std::endl;

        render_state_dirty = result;
        return result;
     }

     bool SceneRenderer::Submit()
     {
        std::cerr << "[SceneRenderer] Submit: target=" << render_target << " dirty=" << render_state_dirty << std::endl;
         if(!render_target||!render_state_dirty)
             return(false);

        bool ok = render_target->Submit();
        std::cerr << "[SceneRenderer] Submit result=" << ok << std::endl;
        return ok;
     }
 }//namespace hgl::graph
