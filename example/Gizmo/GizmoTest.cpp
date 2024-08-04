#include"VulkanAppFramework.h"
#include"Gizmo.h"
#include<hgl/graph/Ray.h>

using namespace hgl;
using namespace hgl::graph;

const Vector3f GizmoPosition(0,0,0);

/**
* 求一个世界坐标在屏幕上的位置
*/
Vector2f WorldToScreen(const Vector3f &world_pos,const CameraInfo &ci,const ViewportInfo &vi)
{
    Vector4f pos=ci.vp*Vector4f(world_pos,1);

    pos/=pos.w;

    return Vector2f((pos.x+1)*vi.GetViewportWidth()/2,(1-pos.y)*vi.GetViewportHeight()/2);
}

class TestApp:public SceneAppFramework
{
    SceneNode root;
    SceneNode *rotate_white_torus=nullptr;

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
        const CameraInfo &ci=camera_control->GetCameraInfo();

        root.Clear();

        root.CreateSubNode(sm_move->GetScene());
        root.CreateSubNode(sm_rotate->GetScene());

        rotate_white_torus=root.CreateSubNode(face_torus);

        root.RefreshTransform();
        render_list->SetCamera(&(camera_control->GetCameraInfo()));
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
        
        const CameraInfo &ci=camera_control->GetCameraInfo();
        const ViewportInfo &vi=GetViewportInfo();

        const float screen_height=vi.GetViewportHeight();

        const Vector4f pos=ci.Project(GizmoPosition);

        {
            Transform tm;

            tm.SetRotation(CalculateFacingRotationQuat(GizmoPosition,ci.view,AxisVector::X));
            tm.SetScale(15);

            rotate_white_torus->SetLocalTransform(tm);
        }

        {
            Transform tm;

            tm.SetScale(pos.w*16.0f/screen_height);

            root.SetLocalTransform(tm);
        }

        root.RefreshTransform();
        render_list->UpdateTransform();

        SceneAppFramework::BuildCommandBuffer(index);
    }
};//class TestApp:public SceneAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1024,1024);
}
