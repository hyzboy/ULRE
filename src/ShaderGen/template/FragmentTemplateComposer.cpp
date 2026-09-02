#include <hgl/mtl/FragmentTemplateComposer.h>

namespace hgl::graph::mtl
{
    bool FragmentTemplateComposer::Compose(
        const ComposeInput &input,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics) const
    {
        if (input.request)
        {
            RenderTemplateValidationDiagnostic diagnostic{};
            if (!ValidateRenderTemplateRequest(*input.request, diagnostic))
                return false;
        }

        const std::string empty_code_module_glsl;
        const std::string &code_module_glsl = input.code_module_glsl
            ? *input.code_module_glsl
            : empty_code_module_glsl;

        // The former assembler remains the single source of byte-stable
        // emission during this migration step. New template implementations
        // replace this delegation one template at a time.
        CompositorAssembler legacy_assembler;
        return legacy_assembler.AssembleDocument(
            input.surface,
            input.pass,
            input.fragment_source,
            input.surface_module,
            input.module_options,
            code_module_glsl,
            out_document,
            out_diagnostics);
    }
}
