// BlendMode Masked ECS - Alpha Cutout Demo
//
// Demonstrates hard alpha cutout (clip/discard) for billboard transparency.
// Pixels whose alpha falls below a threshold are fully discarded by the shader;
// no blending is performed, and depth writes are therefore correct.
//
// GraphicsPipeline: Solid3D base (alpha cutout is entirely shader-side via discard).

#include "IconFreepik.h"
#include "BillboardIconECSBase.h"

class BlendModeMaskedECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "MaskedBillboard_"; }

    const os_char *GetIconTextures(int i) const override { return kIconTextures[i%kIconCount]; }

    void ConfigureQuadPipelineMode() override
    {
        QuadResourcePrepareSystem::SetPipelineForWorld(ecs_context, PipelinePreset::Masked3D);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeMaskedECSApp>(
        OS_TEXT("BlendMode Masked ECS - Alpha Cutout Demo"),
        argc, argv, 1280, 720);
}
