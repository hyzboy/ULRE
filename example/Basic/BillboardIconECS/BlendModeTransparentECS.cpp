// BlendMode Transparent ECS - Alpha Blend Demo
//
// Demonstrates conventional alpha blending for billboard transparency.
// The billboard quad is rendered with a pipeline that has src=SRC_ALPHA,
// dst=ONE_MINUS_SRC_ALPHA blending so semi-transparent regions show through.
//
// Uses built-in InlinePipeline::Alpha3D.

#include "IconFreepik.h"
#include "BillboardIconECSBase.h"

class BlendModeTransparentECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "TransparentBillboard_"; }

    const os_char *GetIconTextures(int i) const override { return kIconTextures[i%kIconCount]; }

    void ConfigureQuadPipelineMode() override
    {
        QuadResourcePrepareSystem::SetPipelineForWorld(ecs_context, InlinePipeline::Alpha3D);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeTransparentECSApp>(
        OS_TEXT("BlendMode Transparent ECS - Alpha Blend Demo"),
        argc, argv, 1280, 720);
}
