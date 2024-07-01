#include"VulkanAppFramework.h"
#include"Gizmo.h"

using namespace hgl;
using namespace hgl::graph;

class TestApp:public SceneAppFramework
{
private:

    bool InitGizmo()
    {
        if(!InitGizmoResource(device))
            return(false);

        return(true);
    }

public:
    
    bool Init(uint w,uint h)
    {        
        if(!SceneAppFramework::Init(w,h))
            return(false);

        if(!InitGizmo())
            return(false);

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
