#pragma once

#include <hgl/mtl/PassProfile.h>

namespace hgl::graph::mtl
{
    // Render-preparation output. Provider roots and include paths are always
    // explicit; this resolver never invents identity or fallback modules.
    struct ScenePassProfile
    {
        ShaderModuleSlotRole roles[MaxPassProfileModuleRoots]{};
        AnsiString module_names[MaxPassProfileModuleRoots];
        AnsiString include_paths[MaxPassProfileModuleRoots];
        uint32 module_count = 0;

        bool AddModule(
            ShaderModuleSlotRole role,
            const AnsiString &module_name,
            const AnsiString &include_path = {}) noexcept;
    };

    ScenePassProfile MakeIdentityForwardLitProfile() noexcept;
    ScenePassProfile MakeForwardUnlitProfile() noexcept;
    ScenePassProfile MakeSkyProfile() noexcept;
    ScenePassProfile MakeShadowCasterProfile() noexcept;

    // Builds a request from an identity explicitly selected by the caller.
    // The template definition supplies its version; this utility only copies
    // the provided scene roots and never selects a rendering policy.
    bool ResolveScenePassProfileRequest(
        PassProfileID profile_id,
        ShaderStage stage,
        const ScenePassProfile &profile,
        PassProfileRequest &out_request,
        PassProfileValidationDiagnostic &out_diagnostic) noexcept;

}
