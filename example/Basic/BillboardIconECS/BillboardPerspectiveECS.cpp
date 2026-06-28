// Billboard Perspective ECS Example - Near Large, Far Small
//
// Demonstrates perspective-correct billboard scaling using 100 icon sprites
// arranged in a spiral.  Billboards always face the camera (FacingTransformSystem).
//
// GraphicsPipeline: standard Solid3D (no blending).

#include "IconFreepik.h"
#include "BillboardIconECSBase.h"

class BillboardPerspectiveECSApp : public BillboardIconECSBase
{
protected:

    const char* GetEntityPrefix() const override { return "BillboardSpiral_"; }

    const os_char *GetIconTextures(int i) const override { return kIconTextures[i%kIconCount]; }

    bool UseFixedPixelSize() const override { return false; }

    void GetWorldSize(float &w, float &h) const override
    {
        w = 8.0f;
        h = 8.0f;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BillboardPerspectiveECSApp>(
        OS_TEXT("Billboard Perspective ECS Example - Near Large, Far Small"),
        argc, argv, 1280, 720);
}
