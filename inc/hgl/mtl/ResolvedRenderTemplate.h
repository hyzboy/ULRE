#pragma once

#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>

namespace hgl::graph::mtl
{
    // Immutable composition identity. A composer consumes this resolved form,
    // rather than independently reinterpreting Definition module paths.
    struct ResolvedRenderTemplate
    {
        const RenderTemplateDefinition *definition = nullptr;
        RenderTemplateRequest request;
        const ShaderCodeModuleDefinition *module_roots[
            MaxRenderTemplateModuleRoots]{};
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

    bool ResolveRenderTemplate(
        const RenderTemplateRequest &request,
        const ShaderCodeModuleRegistry &module_registry,
        ResolvedRenderTemplate &out_template,
        RenderTemplateValidationDiagnostic &out_diagnostic) noexcept;
}
