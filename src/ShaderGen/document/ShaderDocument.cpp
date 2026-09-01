#include <hgl/mtl/ShaderDocument.h>

namespace hgl::graph::mtl
{
    namespace
    {
        static const char *BlockKindName(const ShaderDocumentBlockKind kind)
        {
            switch (kind)
            {
                case ShaderDocumentBlockKind::Version: return "version";
                case ShaderDocumentBlockKind::Extension: return "extension";
                case ShaderDocumentBlockKind::Define: return "define";
                case ShaderDocumentBlockKind::Resource: return "resource";
                case ShaderDocumentBlockKind::Interface: return "interface";
                case ShaderDocumentBlockKind::Module: return "module";
                case ShaderDocumentBlockKind::Function: return "function";
                case ShaderDocumentBlockKind::MainBody: return "main";
                case ShaderDocumentBlockKind::Raw: return "raw";
            }
            return "unknown";
        }

        static void AddDiagnostic(ShaderDocumentDiagnostics &diagnostics,
                                  const char *code,
                                  const char *message,
                                  const int block_index,
                                  const ShaderDocumentSource *source = nullptr)
        {
            ShaderDocumentDiagnostic diagnostic;
            diagnostic.code = code;
            diagnostic.message = message;
            diagnostic.block_index = block_index;
            if (source)
                diagnostic.source = *source;
            ShaderDocumentDiagnostic *item = diagnostics.Create();
            *item = diagnostic;
        }

        static bool IsLegacyVersionBlock(const ShaderDocumentBlock &block)
        {
            return block.kind == ShaderDocumentBlockKind::Raw
                && block.text.Length() >= 8
                && block.text.Left(8) == "#version";
        }
    }

    void ShaderDocument::Clear()
    {
        blocks.Clear();
    }

    int ShaderDocument::GetBlockCount() const
    {
        return blocks.GetCount();
    }

    const ShaderDocumentBlock &ShaderDocument::GetBlock(const int index) const
    {
        return *blocks[index];
    }

    void ShaderDocument::Add(const ShaderDocumentBlockKind kind,
                             const AnsiString &text,
                             const ShaderDocumentSource &source)
    {
        ShaderDocumentBlock *block = blocks.Create();
        block->kind = kind;
        block->text = text;
        block->source = source;
    }

    bool ShaderDocument::Serialize(AnsiString &out_text,
                                   ShaderDocumentDiagnostics &out_diagnostics) const
    {
        out_text = AnsiString();
        out_diagnostics.Clear();

        int version_count = 0;
        int first_non_version = -1;

        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            const ShaderDocumentBlock &block = *blocks[i];
            if (block.kind == ShaderDocumentBlockKind::Version
             || (i == 0 && IsLegacyVersionBlock(block)))
            {
                ++version_count;
                if (first_non_version >= 0)
                    AddDiagnostic(out_diagnostics, "version-order",
                                  "Version block must be the first block", i,
                                  &block.source);
            }
            else if (first_non_version < 0)
            {
                first_non_version = i;
            }

            if (block.text.IsEmpty())
                AddDiagnostic(out_diagnostics, "empty-block",
                              BlockKindName(block.kind), i, &block.source);
        }

        if (version_count > 1)
            AddDiagnostic(out_diagnostics, "duplicate-version",
                          "ShaderDocument contains more than one Version block", -1);

        if (version_count == 0)
            AddDiagnostic(out_diagnostics, "missing-version",
                          "ShaderDocument requires one Version block", -1);

        if (out_diagnostics.GetCount() > 0)
            return false;

        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            const AnsiString &text = blocks[i]->text;
            out_text += text;
        }
        return true;
    }

    bool ShaderDocument::SerializeFragment(
        AnsiString &out_text,
        ShaderDocumentDiagnostics &out_diagnostics) const
    {
        out_text = AnsiString();
        out_diagnostics.Clear();
        for (int i = 0; i < blocks.GetCount(); ++i)
        {
            const ShaderDocumentBlock &block = *blocks[i];
            if (block.text.IsEmpty())
            {
                AddDiagnostic(out_diagnostics, "empty-block",
                              BlockKindName(block.kind), i, &block.source);
                continue;
            }
            out_text += block.text;
        }
        return out_diagnostics.GetCount() == 0;
    }

    hgl::uint64 ShaderDocument::GetSerializedHash(
        ShaderDocumentDiagnostics &out_diagnostics) const
    {
        AnsiString serialized;
        if (!Serialize(serialized, out_diagnostics))
            return 0;

        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(serialized.c_str(),
                           static_cast<size_t>(serialized.Length()));
        return hasher;
    }
}
