#pragma once

#include <hgl/mtl/FixedPipelineVariant.h>
#include <hgl/mtl/RenderTemplate.h>

namespace hgl::graph::mtl
{
    // Render-preparation output. Provider roots and include paths are always
    // explicit; this resolver never invents identity or fallback modules.
    struct SceneRenderTemplateProfile
    {
        ShaderModuleSlotRole roles[MaxRenderTemplateModuleRoots]{};
        AnsiString module_names[MaxRenderTemplateModuleRoots];
        AnsiString include_paths[MaxRenderTemplateModuleRoots];
        uint32 module_count = 0;

        bool AddModule(
            ShaderModuleSlotRole role,
            const AnsiString &module_name,
            const AnsiString &include_path = {}) noexcept;
    };

    SceneRenderTemplateProfile MakeIdentityForwardLitProfile() noexcept;
    SceneRenderTemplateProfile MakeForwardUnlitProfile() noexcept;
    SceneRenderTemplateProfile MakeSkyProfile() noexcept;
    SceneRenderTemplateProfile MakeShadowCasterProfile(bool masked) noexcept;

    bool ResolveShadowCasterRequest(
        bool masked,
        ShaderStage stage,
        const SceneRenderTemplateProfile &profile,
        RenderTemplateRequest &out_request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;

    bool ResolveSceneRenderTemplateRequest(
        const FixedPipelineVariant &variant,
        ShaderStage stage,
        const SceneRenderTemplateProfile &profile,
        RenderTemplateRequest &out_request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;

}
