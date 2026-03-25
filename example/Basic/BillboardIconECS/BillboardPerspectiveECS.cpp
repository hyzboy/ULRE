// Billboard Perspective ECS Example - Near Large, Far Small
//
// Demonstrates perspective-correct billboard scaling using 100 icon sprites
// arranged in a spiral.  Billboards always face the camera (FacingTransformSystem).
//
// Pipeline: standard Solid3D (no blending).

#include "BillboardIconECSBase.h"

class BillboardPerspectiveECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "BillboardSpiral_"; }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BillboardPerspectiveECSApp>(
        OS_TEXT("Billboard Perspective ECS Example - Near Large, Far Small"),
        argc, argv, 1280, 720);
}
