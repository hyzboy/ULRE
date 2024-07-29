﻿#include"VulkanAppFramework.h"
#include"Gizmo.h"

using namespace hgl;
using namespace hgl::graph;

class TestApp:public SceneAppFramework
{
    StaticMesh *sm=nullptr;

private:

    bool InitGizmo()
    {
        if(!InitGizmoResource(device))
            return(false);

        sm=GetGizmoMoveStaticMesh();

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

        render_root.RefreshMatrix();
        render_list->Expend(sm->GetScene());

        return(true);
    }

    ~TestApp()
    {
        FreeGizmoResource();
    }
};//class TestApp:public SceneAppFramework

int main(int,char **)
{
    return RunApp<TestApp>(1280,720);
}
