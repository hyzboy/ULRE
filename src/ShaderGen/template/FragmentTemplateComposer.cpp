#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/CompositorAssembler.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>

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
            if (!ValidateRenderTemplateRequest(
                    *input.request,
                    GetShaderCodeModuleRegistry(),
                    diagnostic))
                return false;
            if (input.variant
             && (input.request->template_id != input.variant->fragment_template
              || input.request->template_version != input.variant->template_version))
                return false;
        }

        const std::string empty_code_module_glsl;
        const std::string &code_module_glsl = input.code_module_glsl
            ? *input.code_module_glsl
            : empty_code_module_glsl;

        // The former assembler remains the single source of byte-stable
        // emission during this migration step. New template implementations
        // replace this delegation one template at a time.
        CompositorAssembler::CompositorModuleOptions legacy_options{};
        legacy_options.sky_module = input.module_options.sky_module;
        legacy_options.direct_lighting_module =
            input.module_options.direct_lighting_module;
        legacy_options.indirect_lighting_module =
            input.module_options.indirect_lighting_module;
        legacy_options.lighting_algorithm_module =
            input.module_options.lighting_algorithm_module;
        legacy_options.material_source_module =
            input.module_options.material_source_module;
        legacy_options.ntb_module = input.module_options.ntb_module;
        legacy_options.forward_lighting_module =
            input.module_options.forward_lighting_module;
        legacy_options.enable_material_source_provider =
            input.module_options.enable_material_source_provider;
        legacy_options.enable_ntb_provider =
            input.module_options.enable_ntb_provider;
        legacy_options.enable_scene_lighting =
            input.module_options.enable_scene_lighting;
        legacy_options.alpha_test = input.module_options.alpha_test;
        legacy_options.alpha_cutoff = input.module_options.alpha_cutoff;
        legacy_options.dither = input.module_options.dither;
        legacy_options.fragment_inputs = input.module_options.fragment_inputs;
        legacy_options.output_contract = input.module_options.output_contract;
        legacy_options.coverage_contract = input.module_options.coverage_contract;

        CompositorAssembler legacy_assembler;
        return legacy_assembler.AssembleDocument(
            input.surface,
            input.pass,
            input.fragment_source,
            input.surface_module,
            legacy_options,
            code_module_glsl,
            out_document,
            out_diagnostics);
    }
}
