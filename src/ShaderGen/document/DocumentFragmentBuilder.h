#pragma once

#include <hgl/mtl/ShaderDocument.h>

namespace hgl::graph::mtl
{
    class DocumentFragmentBuilder
    {
        ShaderDocument &document;
        ShaderDocumentDiagnostics &diagnostics;
        ShaderDocumentSource source;
        int last_block_order = -1;
        bool has_main_body = false;

        bool AddDiagnostic(
            const char *code,
            const char *message,
            const ShaderDocumentSource &block_source);

    public:
        DocumentFragmentBuilder(
            ShaderDocument &target,
            ShaderDocumentDiagnostics &target_diagnostics,
            const ShaderDocumentSource &base_source = {});

        void SetStage(const char *stage);
        void SetMaterial(const char *material);
        bool Add(
            ShaderDocumentBlockKind kind,
            const AnsiString &text,
            const char *logical_name = nullptr,
            const char *module = nullptr,
            const char *path = nullptr);
    };
}
