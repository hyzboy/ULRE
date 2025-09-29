#include<hgl/graph/SceneRenderer.h>
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
        clear_color.Set(0,0,0,1);

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
    }
    
    bool SceneRenderer::RenderFrame()
    {
        if(!GetWorld())
            return(false);

        SceneNode *root = GetWorld()->GetRootNode();
        if(!root)
            return(false);

        root->UpdateWorldTransform();
        const uint renderable = render_task->RebuildRenderList(root);

        LineRenderManager *lrm=GetLineRenderManager();

        if(renderable == 0 && (lrm&&lrm->GetLineCount()==0))
        {
            // nothing to draw this frame
            render_state_dirty = false;
            return true;    // treat as successful no-op
        }

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();

        render_context->BindDescriptor(cmd);

        cmd->SetClearColor(0,clear_color);
        cmd->BeginRenderPass();

        result=render_task->Render(cmd);

        if(lrm)
            lrm->Draw(cmd);

        cmd->EndRenderPass();
        render_target->EndRender();

        render_state_dirty = result;
        return result;
    }

    bool SceneRenderer::Submit()
    {
        if(!render_target||!render_state_dirty)
            return(false);

        return render_target->Submit();
    }
}//namespace hgl::graph
