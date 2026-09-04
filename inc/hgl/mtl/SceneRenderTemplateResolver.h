#pragma once

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
    SceneRenderTemplateProfile MakeShadowCasterProfile() noexcept;

    // Builds a request from an identity explicitly selected by the caller.
    // The template definition supplies its version; this utility only copies
    // the provided scene roots and never selects a rendering policy.
    bool ResolveSceneRenderTemplateRequest(
        RenderTemplateID template_id,
        ShaderStage stage,
        const SceneRenderTemplateProfile &profile,
        RenderTemplateRequest &out_request,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;

}
