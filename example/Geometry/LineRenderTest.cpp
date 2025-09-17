#include<hgl/WorkManager.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/geo/line/LineRenderManager.h>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;

// Forward declare factory function (implemented in LineRenderManager.cpp)
namespace hgl::graph { LineRenderManager *CreateLineRenderManager(RenderFramework *rf); }

class WireShapeTestApp:public WorkObject
{
    LineRenderManager *line_mgr = nullptr;

public:

    using WorkObject::WorkObject;

    bool Init() override
    {
        // Create LineRenderManager via helper
        line_mgr = GetRenderFramework()->GetLineRenderManager();

        if(!line_mgr)
        {
            LogError("WireShapeTestApp::Init: Failed to get LineRenderManager from RenderFramework\n");
            return(false);
        }

        // Setup a larger palette
        line_mgr->SetColor(0,Color4f(1,0,0,1)); // red
        line_mgr->SetColor(1,Color4f(0,1,0,1)); // green
        line_mgr->SetColor(2,Color4f(0,0,1,1)); // blue
        line_mgr->SetColor(3,Color4f(1,1,0,1)); // yellow
        line_mgr->SetColor(4,Color4f(0,1,1,1)); // cyan
        line_mgr->SetColor(5,Color4f(1,0,1,1)); // magenta
        line_mgr->SetColor(6,Color4f(1,1,1,1)); // white
        line_mgr->SetColor(7,Color4f(0.5f,0.5f,0.5f,1)); // gray

        // Add some test lines of different widths
        line_mgr->AddLine(Vector3f(-2,0,0),Vector3f(2,0,0),0,1); // red, width 1
        line_mgr->AddLine(Vector3f(0,-2,0),Vector3f(0,2,0),1,3); // green, width 3
        line_mgr->AddLine(Vector3f(0,0,-2),Vector3f(0,0,2),2,5); // blue, width 5

        // Additional lines: diagonals and offsets
        line_mgr->AddLine(Vector3f(-2,-2,0),Vector3f(2,2,0),3,2); // yellow
        line_mgr->AddLine(Vector3f(-2,2,0),Vector3f(2,-2,0),4,2); // cyan
        line_mgr->AddLine(Vector3f(-3,0,1),Vector3f(3,0,1),5,4); // magenta
        line_mgr->AddLine(Vector3f(0,-3,1),Vector3f(0,3,1),6,4); // white

        // Radial spokes around origin to stress different widths/colors
        const int spokes = 24;
        const float radius = 3.5f;
        for(int i = 0;i < spokes;++i)
        {
            float angle = (2.0f * 3.14159265358979323846f) * (float(i) / float(spokes));
            Vector3f from(std::cos(angle) * 0.5f,std::sin(angle) * 0.5f,0.0f);
            Vector3f to(std::cos(angle) * radius,std::sin(angle) * radius,0.0f);
            uint8_t color_index = uint8_t(i % 8); // cycle through the palette we set up
            uint8_t width = uint8_t(1 + (i % 6)); // widths 1..6
            line_mgr->AddLine(from,to,color_index,width);
        }

        // Position the camera so lines are visible
        CameraControl *cc = GetCameraControl();
        if(cc)
        {
            cc->SetPosition(Vector3f(8,8,8));
            cc->SetTarget(Vector3f(0,0,0));
        }

        return true;
    }
};

int os_main(int,os_char **)
{
    return RunFramework<WireShapeTestApp>(OS_TEXT("Wire Shape Test"),1280,720);
}
