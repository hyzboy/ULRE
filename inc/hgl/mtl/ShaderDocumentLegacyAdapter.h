#pragma once

#include <hgl/mtl/ShaderDocument.h>

namespace hgl::graph::mtl
{
    /// Wraps an already-emitted GLSL string without interpreting or rewriting it.
    ShaderDocument MakeLegacyShaderDocument(
        const AnsiString &legacy_glsl,
        const AnsiString &stage,
        const ShaderDocumentSource &source = {});

    bool IsByteIdenticalLegacyShader(
        const AnsiString &legacy_glsl,
        const ShaderDocument &document,
        ShaderDocumentDiagnostics &out_diagnostics);

    bool BuildInjectedShaderDocument(
        const AnsiString &legacy_glsl,
        const AnsiString &injection,
        const AnsiString &stage,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics);
}
