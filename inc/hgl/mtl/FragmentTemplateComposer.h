#pragma once

#include <hgl/mtl/FixedPipelineVariant.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/ShaderDocument.h>

#include <string>

namespace hgl::graph::mtl
{
    // Transitional template entry point. Its public input is a validated
    // template request; legacy source details remain private migration data
    // until each template owns its complete document emission.
    class FragmentTemplateComposer
    {
    public:
        // Temporary source-path input retained while legacy definitions are
        // migrated. This type intentionally belongs to the new composer, not
        // the legacy compositor implementation.
        struct ModuleOptions
        {
            const char *sky_module = nullptr;
            const char *direct_lighting_module = nullptr;
            const char *indirect_lighting_module = nullptr;
            const char *lighting_algorithm_module = nullptr;
            const char *material_source_module = nullptr;
            const char *ntb_module = nullptr;
            const char *forward_lighting_module = nullptr;
            bool enable_material_source_provider = false;
            bool enable_ntb_provider = false;
            bool enable_scene_lighting = false;
            bool alpha_test = false;
            float alpha_cutoff = 0.5f;
            bool dither = false;
            const hgl::ValueArray<InterStageSemanticContractEntry>
                *fragment_inputs = nullptr;
            const OutputContract *output_contract = nullptr;
            const MaterialCoverageContract *coverage_contract = nullptr;
        };

        struct ComposeInput
        {
            const RenderTemplateRequest *request = nullptr;
            const ResolvedRenderTemplate *resolved_template = nullptr;
            const FixedPipelineVariant *variant = nullptr;
            SurfaceType surface = SurfaceType::Unlit;
            PassType pass = PassType::ForwardOpaque;
            const char *fragment_source = nullptr;
            const char *surface_module = nullptr;
            ModuleOptions module_options;
            const std::string *code_module_glsl = nullptr;
        };

        bool Compose(
            const ComposeInput &input,
            ShaderDocument &out_document,
            ShaderDocumentDiagnostics &out_diagnostics) const;
    };
}
