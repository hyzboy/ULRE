#include "DocumentFragmentBuilder.h"

namespace hgl::graph::mtl
{
    DocumentFragmentBuilder::DocumentFragmentBuilder(
        ShaderDocument &target,
        const ShaderDocumentSource &base_source)
        : document(target), source(base_source)
    {
    }

    void DocumentFragmentBuilder::SetStage(const char *stage)
    {
        source.stage = stage ? stage : "";
    }

    void DocumentFragmentBuilder::SetMaterial(const char *material)
    {
        source.material = material ? material : "";
    }

    void DocumentFragmentBuilder::Add(
        const ShaderDocumentBlockKind kind,
        const AnsiString &text,
        const char *logical_name,
        const char *module,
        const char *path)
    {
        ShaderDocumentSource block_source = source;
        if (logical_name)
            block_source.logical_name = logical_name;
        if (module)
            block_source.module = module;
        if (path)
            block_source.path = path;
        document.Add(kind, text, block_source);
    }
}
