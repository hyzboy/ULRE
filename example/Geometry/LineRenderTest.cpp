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

        // Create concentric layers for widths 1..6; each layer is a ring of radial spokes
        const int spokes = 24;            // number of spokes per layer
        const float inner_radius = 0.5f;  // start of each spoke from center
        const float base_radius = 2.0f;   // base radius for the innermost layer
        const float radius_step = 0.8f;   // how much each layer's radius increases
        const float z_step = 0.5f;       // z offset between layers so they don't overlap exactly

        for(int width = 1; width <= 16; ++width)
        {
            float radius = base_radius ;
            float z = (width%2?1:-1)*(width - 1) * z_step; // layer height
            uint8_t color_index = uint8_t((width - 1) % 8);
            uint8_t w = uint8_t(width);

            for(int i = 0; i < spokes; ++i)
            {
                float angle = (2.0f * 3.14159265358979323846f) * (float(i) / float(spokes));

                math::Vector3f from(std::cos(angle) * inner_radius, std::sin(angle) * inner_radius, z);
                math::Vector3f to  (std::cos(angle) * radius,       std::sin(angle) * radius,       z);

                line_mgr->AddLine(from, to, color_index, w);
            }
        }

        // Position the camera so lines are visible
        CameraControl *cc = GetCameraControl();
        if(cc)
        {
            cc->SetPosition(math::Vector3f(8,8,8));
            cc->SetTarget(math::Vector3f(0,0,0));
        }

        return true;
    }
};

int os_main(int,os_char **)
{
    return RunFramework<WireShapeTestApp>(OS_TEXT("Wire Shape Test"),1280,720);
}
