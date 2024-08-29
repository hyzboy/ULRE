#include"VulkanAppFramework.h"
#include"Gizmo.h"
#include<hgl/graph/Ray.h>

using namespace hgl;
using namespace hgl::graph;

const Vector3f GizmoPosition(0,0,0);
//
///**
//* 一种永远转向正面的场景节点
//*/
//class BillboardSceneNode:public SceneNode
//{
//    CameraInfo *camera_info=nullptr;
//    bool face_to_camera=false;
//
//    ViewportInfo *viewport_info=nullptr;
//    float fixed_scale=1.0;
//
//public:
//
//    using SceneNode::SceneNode;
//    virtual ~BillboardSceneNode()=default;
//
//    virtual void SetCameraInfo  (CameraInfo *   ci  ){camera_info   =ci;}
//    virtual void SetViewportInfo(ViewportInfo * vi  ){viewport_info =vi;}
//
//    virtual void SetFaceToCamera(bool           ftc ){face_to_camera=ftc;}
//    virtual void SetFixedScale  (const float    size){fixed_scale   =size;}
//
//    virtual bool RefreshTransform(const Transform &tf=IdentityTransform) override
//    {
//        if(!camera_info)
//        {
//            return SceneNode::RefreshTransform(tf);
//        }
//
//        if(face_to_camera)
//        {
//            LocalTransform.SetRotation(CalculateFacingRotationQuat(GetWorldPosition(),camera_info->view,AxisVector::X));
//        }
//
//        if(viewport_info)
//        {
//            const float screen_height=viewport_info->GetViewportHeight();
//
//            const Vector4f pos=camera_info->Project(GetWorldPosition());
//
//            LocalTransform.SetScale(pos.w*fixed_scale/screen_height);
//        }
//
//        return SceneNode::RefreshTransform(tf);
//    }
//};//class BillboardSceneNode:public SceneNode

class TestApp:public SceneAppFramework
{
    SceneNode root;
    //BillboardSceneNode *rotate_white_torus=nullptr;

    StaticMesh *sm_move=nullptr;
    StaticMesh *sm_rotate=nullptr;

    Renderable *face_torus=nullptr;

private:

    bool InitGizmo()
    {
        if(!InitGizmoResource(db))
            return(false);

        sm_move     =GetGizmoMoveStaticMesh();
        sm_rotate   =GetGizmoRotateStaticMesh();

        face_torus  =GetGizmoRenderable(GizmoShape::Torus,GizmoColor::White);

        return(true);
    }

    void InitGizmoSceneTree()
    {
        camera_control->Refresh();
        CameraInfo *ci=camera_control->GetCameraInfo();

        root.Clear();

        root.CreateSubNode(sm_move->GetScene());
        root.CreateSubNode(sm_rotate->GetScene());

        //{
        //    Transform tm;

        //    tm.SetScale(7.5);

        //    rotate_white_torus=new BillboardSceneNode(tm,face_torus);

        //    rotate_white_torus->SetCameraInfo(ci);
        //    rotate_white_torus->SetFaceToCamera(true);

        //    root.AddSubNode(rotate_white_torus);
        //}

        root.RefreshMatrix();
        render_list->SetCamera(ci);
        render_list->Expend(&root);
    }

public:

    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitGizmo())
            return(false);

        InitGizmoSceneTree();
        
        camera->pos=Vector3f(32,32,32);
        camera_control->SetTarget(Vector3f(0,0,0));

        return(true);
    }

    ~TestApp()
    {
        FreeGizmoResource();
    }
    
    void BuildCommandBuffer(uint32 index) override
    {
        camera_control->Refresh();
        
        const CameraInfo *ci=camera_control->GetCameraInfo();
        const ViewportInfo *vi=GetViewportInfo();

        const float screen_height=vi->GetViewportHeight();

        const Vector4f pos=ci->Project(GizmoPosition);

        //{
        //    Transform tm;

        //    tm.SetScale(pos.w*16.0f/screen_height);

        //    root.SetLocalTransform(tm);
        //}

        root.RefreshMatrix();
        render_list->UpdateLocalToWorld();

        SceneAppFramework::BuildCommandBuffer(index);
    }
};//class TestApp:public SceneAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1024,1024);
}
