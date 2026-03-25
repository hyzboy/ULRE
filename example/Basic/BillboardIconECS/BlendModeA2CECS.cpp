// BlendMode Alpha-to-Coverage ECS - MSAA A2C Demo
//
// Demonstrates alpha-to-coverage (A2C): the fragment shader outputs the surface alpha
// and the GPU's MSAA hardware converts it into coverage bits at sample boundaries.
// This gives smooth stochastic transparency without requiring sorted rendering.
//
// Pipeline: Solid3D base + alphaToCoverageEnable = VK_TRUE + MSAA x4.
//
// NOTE: The A2C PipelineData is constructed below via CreateA2CPipelineData().
// TODO: wire it into QuadResourcePrepareSystem once the ECS billboard path supports
//       per-system custom PipelineData overrides.

#include<hgl/vk/pipeline/VKPipelineData.h>
#include "BillboardIconECSBase.h"

class BlendModeA2CECSApp : public BillboardIconECSBase
{
protected:
    const char* GetEntityPrefix() const override { return "A2CBillboard_"; }

private:
    /**
     * Build a PipelineData that enables alphaToCoverage for the billboard pass.
     * Cloned from the Solid3D preset then modified.
     */
    PipelineData* CreateA2CPipelineData()
    {
        const PipelineData* base = GetPipelineData(InlinePipeline::Solid3D);
        if (!base) return nullptr;

        PipelineData* pd = new PipelineData(base);
        pd->SetSamleCount(VK_SAMPLE_COUNT_4_BIT);
        pd->multi_sample->alphaToCoverageEnable = VK_TRUE;
        return pd;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BlendModeA2CECSApp>(
        OS_TEXT("BlendMode Alpha-to-Coverage ECS - MSAA A2C Demo"),
        argc, argv, 1280, 720);
}
