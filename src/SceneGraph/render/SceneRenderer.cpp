#include<hgl/graph/SceneRenderer.h>
#include<hgl/graph/Scene.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::graph
{
    SceneRenderer::SceneRenderer(RenderFramework *rf,IRenderTarget *rt)
    {
        render_target=rt;
        scene=nullptr;

        camera_control=nullptr;

        render_task=new RenderTask("DefaultRenderTask",rt);

        clear_color.Set(0,0,0,1);
    }

    SceneRenderer::~SceneRenderer()
    {
        delete render_task;
    }

    bool SceneRenderer::SetRenderTarget(IRenderTarget *rt)
    {
        if(render_target==rt)
            return(true);

        render_target=rt;

        if(camera_control)
            camera_control->SetViewport(render_target->GetViewportInfo());

        render_task->SetRenderTarget(rt);

        return(true);
    }

    void SceneRenderer::SetScene(Scene *sw)
    {
        if(scene==sw)
            return;

        scene=sw;

        if(camera_control)
            scene->SetCameraControl(camera_control);
    }

    void SceneRenderer::SetCameraControl(CameraControl *cc)
    {
        camera_control=cc;

        if(camera_control&&render_target)
            camera_control->SetViewport(render_target->GetViewportInfo());

        if(scene)
            scene->SetCameraControl(camera_control);

        render_task->SetCameraInfo(GetCameraInfo());
    }

    void SceneRenderer::Tick(double delta)
    {
        scene->Tick(delta);
    }
    
    bool SceneRenderer::RenderFrame()
    {
        if(!scene)
            return(false);

        SceneNode *root = scene->GetRootNode();

        if(!root)
            return(false);

        root->UpdateWorldTransform();

        // 这里内部会将Scene tree展开成RenderCollector,而RenderCollector排序是需要CameraInfo的
        render_task->RebuildRenderList(root);

        bool result = false;

        RenderCmdBuffer *cmd = render_target->BeginRender();

        scene->BindDescriptor(cmd);

        cmd->SetClearColor(0,clear_color);

        cmd->BeginRenderPass();

        result=render_task->Render(cmd);

        scene->RenderLines(cmd);

        cmd->EndRenderPass();

        render_target->EndRender();

        render_state_dirty = result;

        return(result);
    }

    bool SceneRenderer::Submit()
    {
        if(!render_target)
            return(false);

        if(!render_state_dirty)
            return(false);

        return render_target->Submit();
    }
}//namespace hgl::graph
