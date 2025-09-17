#include<hgl/WorkManager.h>
#include<hgl/graph/VKCommandBuffer.h>
#include "LineRenderManager.h"
#include<iostream>

using namespace hgl;
using namespace hgl::graph;

// Forward declare factory function (implemented in LineRenderManager.cpp)
namespace hgl::graph { LineRenderManager *CreateLineRenderManager(RenderFramework *rf); }

class WireShapeTestApp:public WorkObject
{
    LineRenderManager *line_mgr=nullptr;

public:
    using WorkObject::WorkObject;

    ~WireShapeTestApp()
    {
        SAFE_CLEAR(line_mgr);
    }

    bool Init() override
    {
        // Create LineRenderManager via helper
        line_mgr = hgl::graph::CreateLineRenderManager(GetRenderFramework());
        if(!line_mgr)
        {
            std::cerr<<"Failed to create LineRenderManager"<<std::endl;
            return(false);
        }

        // Setup a small palette
        line_mgr->SetColor(0, Color4f(1,0,0,1)); // red
        line_mgr->SetColor(1, Color4f(0,1,0,1)); // green
        line_mgr->SetColor(2, Color4f(0,0,1,1)); // blue

        // Add some test lines of different widths
        line_mgr->AddLine(Vector3f(-2,0,0), Vector3f(2,0,0), 0, 1); // red, width 1
        line_mgr->AddLine(Vector3f(0,-2,0), Vector3f(0,2,0), 1, 3); // green, width 3
        line_mgr->AddLine(Vector3f(0,0,-2), Vector3f(0,0,2), 2, 5); // blue, width 5

        // Position the camera so lines are visible
        CameraControl *cc = GetCameraControl();
        if(cc)
        {
            cc->SetPosition(Vector3f(8,8,8));
            cc->SetTarget(Vector3f(0,0,0));
        }

        return true;
    }

    // Render only draws the line manager into the swapchain render target for this simple test.
    void Render(double) override
    {
        if(!line_mgr)
            return;

        RenderFramework *rf = GetRenderFramework();
        if(!rf)
            return;

        SwapchainRenderTarget *srt = rf->GetSwapchainRenderTarget();
        if(!srt)
            return;

        // Acquire next image and begin render for current frame
        if(!srt->NextFrame())
        {
            // Could not acquire next image
            return;
        }

        RenderCmdBuffer *cmd = srt->BeginRender();
        if(!cmd)
            return;

        // Optionally bind camera/scene descriptor bindings if needed
        CameraControl *cc = GetCameraControl();
        if(cc)
        {
            cmd->SetDescriptorBinding(cc->GetDescriptorBinding());
        }

        // Clear color default
        cmd->SetClearColor(0, Color4f(0,0,0,1));

        cmd->BeginRenderPass();
            // Draw lines
            line_mgr->Draw(cmd);
        cmd->EndRenderPass();

        srt->EndRender();

        // Submit and present
        srt->Submit();
    }
};

int os_main(int,os_char **)
{
    return RunFramework<WireShapeTestApp>(OS_TEXT("Wire Shape Test"),1280,720);
}
