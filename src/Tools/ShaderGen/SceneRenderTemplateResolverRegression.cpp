#include <hgl/mtl/SceneRenderTemplateResolver.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>

using namespace hgl::graph::mtl;

int main()
{
    FixedPipelineVariant variant{};
    variant.fragment_template = RenderTemplateID::ForwardUnlit;
    variant.template_version = 1;

    SceneRenderTemplateProfile profile;
    if (!profile.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "material_surface",
            "surface/material_surface.glsl")
     || !profile.AddModule(
            ShaderModuleSlotRole::OutputPolicy, "forward_lighting",
            "compositor/forward_lighting.glsl"))
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
    ShaderCodeModuleDefinition modules[2]{};
    modules[0].name = "material_surface";
    modules[0].glsl_code = "";
    modules[0].slot_role = ShaderModuleSlotRole::SurfaceProvider;
    modules[1].name = "forward_lighting";
    modules[1].glsl_code = "";
    modules[1].slot_role = ShaderModuleSlotRole::OutputPolicy;
    ShaderCodeModuleRegistry registry;
    if (!registry.Register(modules[0]) || !registry.Register(modules[1]))
        return 5;
    ResolvedRenderTemplate resolved{};
    if (!ResolveRenderTemplate(request, registry, resolved, diagnostic)
     || !resolved.IsValid()
     || resolved.request.module_roots[0].include_path
            != "surface/material_surface.glsl")
        return 6;
    FragmentTemplateComposer composer;
    FragmentTemplateComposer::ComposeInput compose_input{};
    compose_input.request = &request;
    compose_input.resolved_template = &resolved;
    ShaderDocument document;
    ShaderDocumentDiagnostics document_diagnostics;
    if (!composer.Compose(
            compose_input, document, document_diagnostics)
     || document.GetBlockCount() == 0)
        return 7;
    const SceneRenderTemplateProfile sky_profile = MakeSkyProfile();
    if (sky_profile.module_count != 2
     || sky_profile.roles[0] != ShaderModuleSlotRole::AmbientLightProvider)
        return 9;
    const SceneRenderTemplateProfile shadow_profile =
        MakeShadowCasterProfile(true);
    if (shadow_profile.module_count != 2
     || shadow_profile.roles[0] != ShaderModuleSlotRole::SurfaceProvider)
        return 10;

    SceneRenderTemplateProfile duplicate;
    SceneRenderTemplateProfile missing_path;
    if (missing_path.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "material_surface"))
        return 8;
    if (!duplicate.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/a",
            "surface/a.glsl")
     || duplicate.AddModule(
            ShaderModuleSlotRole::SurfaceProvider, "surface/b",
            "surface/b.glsl"))
        return 4;
    return 0;
}
