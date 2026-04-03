// BlendMode Alpha-to-Coverage ECS - MSAA A2C Demo
//
// Demonstrates alpha-to-coverage (A2C): the fragment shader outputs the surface alpha
// and the GPU's MSAA hardware converts it into coverage bits at sample boundaries.
// This gives smooth stochastic transparency without requiring sorted rendering.
//
// GraphicsPipeline: built-in PipelinePreset::AlphaToCoverage3D (MSAA x4 + A2C).

#include "IconGradient.h"
#include "BillboardIconECSBase.h"

class BlendModeA2CECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "A2CBillboard_"; }

    const os_char *GetIconTextures(int i) const override { return kIconTextures[i%kIconCount]; }

    void ConfigureQuadPipelineMode() override
    {
        QuadResourcePrepareSystem::SetPipelineForWorld(ecs_context, PipelinePreset::AlphaToCoverage3D);
        QuadResourcePrepareSystem::SetChannelHintForWorld(ecs_context, TextureChannelHint::Grayscale);
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeA2CECSApp>(
        OS_TEXT("BlendMode Alpha-to-Coverage ECS - MSAA A2C Demo"),
        argc, argv, 1280, 720);
}
