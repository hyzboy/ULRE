#include <hgl/mtl/ShaderDocument.h>
#include <hgl/mtl/ShaderDocumentLegacyAdapter.h>

using namespace hgl::graph::mtl;
using hgl::AnsiString;

int main()
{
    ShaderDocument document;
    document.Add(ShaderDocumentBlockKind::Version, "#version 460");
    document.Add(ShaderDocumentBlockKind::Define, "#define TEST_DEFINE 1");
    document.Add(ShaderDocumentBlockKind::Raw, "void main() {}");

    AnsiString serialized;
    ShaderDocumentDiagnostics diagnostics;
    if (!document.Serialize(serialized, diagnostics))
        return 1;
    if (serialized.IsEmpty() || diagnostics.GetCount() != 0)
        return 6;
    ShaderDocumentDiagnostics fragment_diagnostics;
    AnsiString fragment;
    if (!document.SerializeFragment(fragment, fragment_diagnostics)
     || fragment != serialized)
        return 9;
    const hgl::uint64 first_hash = document.GetSerializedHash(diagnostics);
    const hgl::uint64 second_hash = document.GetSerializedHash(diagnostics);
    if (first_hash != second_hash)
        return 5;

    ShaderDocument invalid;
    invalid.Add(ShaderDocumentBlockKind::Raw, "raw");
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460");
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460");
    if (invalid.Serialize(serialized, diagnostics)
     || diagnostics.GetCount() < 2)
        return 2;

    const AnsiString legacy = "#version 460\nvoid main() {}\n";
    ShaderDocument legacy_document = MakeLegacyShaderDocument(legacy, "fragment");
    if (!IsByteIdenticalLegacyShader(legacy, legacy_document, diagnostics)
     || legacy_document.GetBlockCount() != 1
     || legacy_document.GetBlock(0).source.stage != "fragment")
        return 3;

    const AnsiString legacy_without_newline = "#version 460";
    ShaderDocument exact_document = MakeLegacyShaderDocument(
        legacy_without_newline, "mesh");
    if (!IsByteIdenticalLegacyShader(
            legacy_without_newline, exact_document, diagnostics))
        return 4;

    ShaderDocument injected;
    if (!BuildInjectedShaderDocument(
            "#version 460\nvoid main() {}\n",
            "#define INJECTED 1\n",
            "mesh",
            injected,
            diagnostics))
        return 7;
    if (!IsByteIdenticalLegacyShader(
            "#version 460\n#define INJECTED 1\nvoid main() {}\n",
            injected,
            diagnostics))
        return 8;

    return 0;
}
