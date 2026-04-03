// BlendMode Dither ECS - Bayer Ordered Dither Demo
//
// Demonstrates Bayer ordered dithering applied to billboard transparency.
// The material uses a dither-pattern technique so that semi-transparent pixels
// appear as a screen-space stipple pattern without requiring alpha blending.
//
// GraphicsPipeline: Solid3D base (dithering is entirely shader-side).

#include "IconGradient.h"
#include "BillboardIconECSBase.h"

class BlendModeDitherECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "DitherBillboard_"; }

    const os_char *GetIconTextures(int i) const override { return kIconTextures[i%kIconCount]; }

    void ConfigureQuadPipelineMode() override
    {
        QuadResourcePrepareSystem::SetPipelineForWorld(ecs_context, InlinePipeline::Dither3D);
        QuadResourcePrepareSystem::SetChannelHintForWorld(ecs_context, TextureChannelHint::Grayscale);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeDitherECSApp>(
        OS_TEXT("BlendMode Dither ECS - Bayer Ordered Dither Demo"),
        argc, argv, 1280, 720);
}
