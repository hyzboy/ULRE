#pragma once

#include <hgl/mtl/FixedPipelineVariant.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/RenderTemplate.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/ShaderDocument.h>

#include <string>

namespace hgl::graph::mtl
{
    // Template-first fragment emission entry point.
    // Normal callers must provide a validated RenderTemplateRequest or
    // ResolvedRenderTemplate; implicit auto-composition is intentionally
    // disabled.
    class FragmentTemplateComposer
    {
    public:
        struct ComposeInput
        {
            const RenderTemplateRequest *request = nullptr;
            const ResolvedRenderTemplate *resolved_template = nullptr;
            const FixedPipelineVariant *variant = nullptr;
            bool alpha_test = false;
            float alpha_cutoff = 0.5f;
            bool dither = false;
            const hgl::ValueArray<InterStageSemanticContractEntry>
                *fragment_inputs = nullptr;
            const OutputContract *output_contract = nullptr;
            const MaterialCoverageContract *coverage_contract = nullptr;
            const std::string *code_module_glsl = nullptr;
        };

        bool Compose(
            const ComposeInput &input,
            ShaderDocument &out_document,
            ShaderDocumentDiagnostics &out_diagnostics) const;
    };
}
