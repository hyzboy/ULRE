#pragma once

#include <hgl/mtl/CompositorAssembler.h>
#include <hgl/mtl/FixedPipelineVariant.h>
#include <hgl/mtl/RenderTemplate.h>

namespace hgl::graph::mtl
{
    // Transitional template entry point. Its public input is a validated
    // template request; legacy source details remain private migration data
    // until each template owns its complete document emission.
    class FragmentTemplateComposer
    {
    public:
        struct ComposeInput
        {
            const RenderTemplateRequest *request = nullptr;
            const FixedPipelineVariant *variant = nullptr;
            SurfaceType surface = SurfaceType::Unlit;
            PassType pass = PassType::ForwardOpaque;
            const char *fragment_source = nullptr;
            const char *surface_module = nullptr;
            CompositorAssembler::CompositorModuleOptions module_options;
            const std::string *code_module_glsl = nullptr;
        };

        bool Compose(
            const ComposeInput &input,
            ShaderDocument &out_document,
            ShaderDocumentDiagnostics &out_diagnostics) const;
    };
}
