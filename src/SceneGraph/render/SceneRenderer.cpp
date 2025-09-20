#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_target=rt;
        render_context=new RenderContext(rf,rt);
        render_task=new RenderTask("DefaultRenderTask",rt);
        clear_color.Set(0,0,0,1);
    }

    SceneRenderer::~SceneRenderer()
    {
        SAFE_CLEAR(render_task);
        SAFE_CLEAR(render_context);
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        render_target=rt;

        if(render_context)
            render_context->SetRenderTarget(rt);

        render_task->SetRenderTarget(rt);
        return(true);
    }

    void SceneRenderer::SetScene(Scene *sw)
    {
        if(!render_context)return;
        render_context->SetScene(sw);
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        if(!render_context)return;
        render_context->SetCameraControl(cc);
        render_task->SetCameraInfo(GetCameraInfo());
    }

    void SceneRenderer::Tick(double delta)
    {
        if(render_context)
            render_context->Tick(delta);

        if(GetScene())
            GetScene()->Tick(delta);
    }
    
    bool SceneRenderer::RenderFrame()
    {
        if(!GetScene())
            return(false);

        SceneNode *root = GetScene()->GetRootNode();
        if(!root)
            return(false);

        root->UpdateWorldTransform();
        render_task->RebuildRenderList(root);

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();

        render_context->BindDescriptor(cmd);

        cmd->SetClearColor(0,clear_color);
        cmd->BeginRenderPass();

        result=render_task->Render(cmd);

        if(GetLineRenderManager())
            GetLineRenderManager()->Draw(cmd);

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
