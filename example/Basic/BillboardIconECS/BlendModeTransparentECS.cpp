// BlendMode Transparent ECS - Alpha Blend Demo
//
// Demonstrates conventional alpha blending for billboard transparency.
// The billboard quad is rendered with a pipeline that has src=SRC_ALPHA,
// dst=ONE_MINUS_SRC_ALPHA blending so semi-transparent regions show through.
//
// TODO: wire the Alpha3D pipeline into the QuadResourcePrepareSystem to enable
//       proper per-entity alpha blending.  Currently uses Solid3D as a placeholder.

#include "BillboardIconECSBase.h"

class BlendModeTransparentECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "TransparentBillboard_"; }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeTransparentECSApp>(
        OS_TEXT("BlendMode Transparent ECS - Alpha Blend Demo"),
        argc, argv, 1280, 720);
}
