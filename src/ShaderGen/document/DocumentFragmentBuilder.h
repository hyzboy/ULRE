#pragma once

#include <hgl/mtl/ShaderDocument.h>

namespace hgl::graph::mtl
{
    class DocumentFragmentBuilder
    {
        ShaderDocument &document;
        ShaderDocumentSource source;

    public:
        DocumentFragmentBuilder(
            ShaderDocument &target,
            const ShaderDocumentSource &base_source = {});

        void SetStage(const char *stage);
        void SetMaterial(const char *material);
        void Add(
            ShaderDocumentBlockKind kind,
            const AnsiString &text,
            const char *logical_name = nullptr,
            const char *module = nullptr,
            const char *path = nullptr);
    };
}
