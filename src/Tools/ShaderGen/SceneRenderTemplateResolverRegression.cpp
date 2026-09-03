#include <hgl/mtl/SceneRenderTemplateResolver.h>

using namespace hgl::graph::mtl;

int main()
{
    FixedPipelineVariant variant{};
    variant.fragment_template = RenderTemplateID::ForwardUnlit;
    variant.template_version = 1;

    SceneRenderTemplateProfile profile;
    if (!profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/material_surface")
     || !profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy, "output/forward"))
        return 1;

    RenderTemplateRequest request;
    RenderTemplateValidationDiagnostic diagnostic{};
    if (!ResolveSceneRenderTemplateRequest(
            variant, hgl::graph::ShaderStage::Fragment,
            profile, request, diagnostic))
        return 2;
    if (request.template_id != RenderTemplateID::ForwardUnlit
     || request.module_root_count != 2)
        return 3;

    SceneRenderTemplateProfile duplicate;
    if (!duplicate.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/a")
     || duplicate.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/b"))
        return 4;
    return 0;
}
