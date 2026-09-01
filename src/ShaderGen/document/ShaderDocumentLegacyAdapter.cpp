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
}
