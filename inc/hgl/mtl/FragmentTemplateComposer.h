#pragma once

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
    // Normal callers provide a resolved template. Validation and module-graph
    // resolution happen once before native document emission.
    class FragmentTemplateComposer
    {
    public:
        struct ComposeInput
        {
            const ResolvedRenderTemplate *resolved_template = nullptr;
            bool alpha_test = false;
            float alpha_cutoff = 0.5f;
            bool dither = false;
            const hgl::ValueArray<InterStageSemanticContractEntry>
                *fragment_inputs = nullptr;
            const OutputContract *output_contract = nullptr;
            const MaterialCoverageContract *coverage_contract = nullptr;
            const ShaderDocument *code_module_document = nullptr;
        };

        bool Compose(
            const ComposeInput &input,
            ShaderDocument &out_document,
            ShaderDocumentDiagnostics &out_diagnostics) const;
    };
}
