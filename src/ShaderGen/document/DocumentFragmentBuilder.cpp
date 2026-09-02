#include "DocumentFragmentBuilder.h"

namespace hgl::graph::mtl
{
    namespace
    {
        int GetBlockOrder(const ShaderDocumentBlockKind kind)
        {
            switch (kind)
            {
            case ShaderDocumentBlockKind::Version:  return 0;
            case ShaderDocumentBlockKind::Extension:return 1;
            case ShaderDocumentBlockKind::Define:   return 2;
            case ShaderDocumentBlockKind::Resource: return 3;
            case ShaderDocumentBlockKind::Interface:return 4;
            case ShaderDocumentBlockKind::Module:   return 5;
            case ShaderDocumentBlockKind::Function: return 6;
            case ShaderDocumentBlockKind::MainBody: return 7;
            case ShaderDocumentBlockKind::Raw:      return 8;
            }
            return -1;
        }
    }

    DocumentFragmentBuilder::DocumentFragmentBuilder(
        ShaderDocument &target,
        ShaderDocumentDiagnostics &target_diagnostics,
        const ShaderDocumentSource &base_source)
        : document(target), diagnostics(target_diagnostics), source(base_source)
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

    bool DocumentFragmentBuilder::AddDiagnostic(
        const char *code,
        const char *message,
        const ShaderDocumentSource &block_source)
    {
        ShaderDocumentDiagnostic *diagnostic = diagnostics.Create();
        diagnostic->code = code;
        diagnostic->message = message;
        diagnostic->block_index = document.GetBlockCount();
        diagnostic->source = block_source;
        return false;
    }

    bool DocumentFragmentBuilder::Add(
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

        if (text.IsEmpty())
            return AddDiagnostic(
                "empty-block",
                "DocumentFragmentBuilder refuses empty blocks",
                block_source);

        const int block_order = GetBlockOrder(kind);
        if (block_order < last_block_order)
            return AddDiagnostic(
                "block-order",
                "ShaderDocument block order is invalid",
                block_source);

        if (kind == ShaderDocumentBlockKind::MainBody)
        {
            if (has_main_body)
                return AddDiagnostic(
                    "duplicate-main",
                    "ShaderDocument contains more than one MainBody block",
                    block_source);
            has_main_body = true;
        }

        document.Add(kind, text, block_source);
        last_block_order = block_order;
        return true;
    }
}
