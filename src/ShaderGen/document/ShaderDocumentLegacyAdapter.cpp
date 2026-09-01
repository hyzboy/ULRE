#include <hgl/mtl/ShaderDocumentLegacyAdapter.h>

namespace hgl::graph::mtl
{
    ShaderDocument MakeLegacyShaderDocument(
        const AnsiString &legacy_glsl,
        const AnsiString &stage,
        const ShaderDocumentSource &source)
    {
        ShaderDocument document;
        ShaderDocumentSource block_source = source;
        block_source.stage = stage;
        document.Add(ShaderDocumentBlockKind::Raw, legacy_glsl, block_source);
        return document;
    }

    bool IsByteIdenticalLegacyShader(
        const AnsiString &legacy_glsl,
        const ShaderDocument &document,
        ShaderDocumentDiagnostics &out_diagnostics)
    {
        AnsiString serialized;
        if (!document.Serialize(serialized, out_diagnostics))
            return false;

        if (serialized != legacy_glsl)
        {
            ShaderDocumentDiagnostic *diagnostic = out_diagnostics.Create();
            diagnostic->code = "legacy-byte-mismatch";
            diagnostic->message = "Serialized ShaderDocument differs from legacy GLSL";
            diagnostic->block_index = -1;
            return false;
        }
        return true;
    }

    bool BuildInjectedShaderDocument(
        const AnsiString &legacy_glsl,
        const AnsiString &injection,
        const AnsiString &stage,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics)
    {
        out_document.Clear();
        out_diagnostics.Clear();

        const ShaderDocumentSource source = { {}, stage, {}, {}, {} };
        AnsiString serialized;
        if (injection.IsEmpty())
        {
            out_document.Add(ShaderDocumentBlockKind::Raw, legacy_glsl, source);
            return out_document.Serialize(serialized, out_diagnostics);
        }

        const int version_end = legacy_glsl.FindChar('\n');
        if (version_end < 0)
        {
            out_document.Add(ShaderDocumentBlockKind::Raw,
                             legacy_glsl + "\n" + injection, source);
        }
        else
        {
            out_document.Add(ShaderDocumentBlockKind::Raw,
                             legacy_glsl.Left(version_end + 1), source);
            out_document.Add(ShaderDocumentBlockKind::Raw, injection, source);
            out_document.Add(ShaderDocumentBlockKind::Raw,
                             legacy_glsl.SubString(
                                 version_end + 1,
                                 legacy_glsl.Length() - version_end - 1),
                             source);
        }

        return out_document.Serialize(serialized, out_diagnostics);
    }
}
