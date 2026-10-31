#pragma once

#include <hgl/mtl/PassProfile.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>

namespace hgl::graph::mtl
{
    // Immutable composition identity. A composer consumes this resolved form,
    // rather than independently reinterpreting Definition module paths.
    struct ResolvedPassProfile
    {
        const PassProfileDefinition *definition = nullptr;
        PassProfileRequest request;
        const GlslModuleDefinition *module_roots[
            MaxPassProfileModuleRoots]{};
        uint32 module_root_count = 0;
        ShaderCodeResourceManifest manifest;
        uint64 stable_hash = 0;

        bool IsValid() const noexcept
        {
            return definition
                && module_root_count == request.module_root_count
                && manifest.IsValid()
                && stable_hash != 0;
        }
    };

    bool ResolvePassProfile(
        const PassProfileRequest &request,
        const GlslModuleRegistry &module_registry,
        ResolvedPassProfile &out_profile,
        PassProfileValidationDiagnostic &out_diagnostic) noexcept;
}
