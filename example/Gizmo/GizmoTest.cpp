#include"VulkanAppFramework.h"
#include"Gizmo.h"

using namespace hgl;
using namespace hgl::graph;

class TestApp:public SceneAppFramework
{
    SceneNode root;

    StaticMesh *sm_move=nullptr;
    StaticMesh *sm_rotate=nullptr;

private:

    bool InitGizmo()
    {
        if(!InitGizmoResource(db))
            return(false);

        sm_move     =GetGizmoMoveStaticMesh();
        sm_rotate   =GetGizmoRotateStaticMesh();

        return(true);
    }

public:
    
    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitGizmo())
            return(false);
        
        camera->pos=Vector3f(32,32,32);
        camera_control->SetTarget(Vector3f(0,0,0));
        camera_control->Refresh();

        root.CreateSubNode(sm_move->GetScene());
        root.CreateSubNode(sm_rotate->GetScene());

        root.RefreshMatrix();
        render_list->Expend(&root);

        return(true);
    }

    ~TestApp()
    {
        FreeGizmoResource();
    }
};//class TestApp:public SceneAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1024,1024);
}
