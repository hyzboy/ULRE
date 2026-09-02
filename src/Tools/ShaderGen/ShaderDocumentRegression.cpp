#include <hgl/mtl/ShaderDocument.h>
#include <hgl/mtl/ShaderDocumentLegacyAdapter.h>
#include <hgl/mtl/ShaderLegacyAuditShell.h>
#include "../../ShaderGen/document/DocumentFragmentBuilder.h"

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
    ShaderDocumentSource invalid_source;
    invalid_source.stage = "fragment";
    invalid.Add(ShaderDocumentBlockKind::Raw, "raw");
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460", invalid_source);
    invalid.Add(ShaderDocumentBlockKind::Version, "#version 460", invalid_source);
    if (invalid.Serialize(serialized, diagnostics)
     || diagnostics.GetCount() < 2)
        return 2;
    if (diagnostics[0]->source.stage.IsEmpty())
        return 10;

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

    ShaderDocument builder_document;
    ShaderDocumentDiagnostics builder_diagnostics;
    DocumentFragmentBuilder builder(builder_document, builder_diagnostics);
    if (!builder.Add(ShaderDocumentBlockKind::Version, "#version 460\n")
     || !builder.Add(ShaderDocumentBlockKind::Define, "#define TEST 1\n")
     || !builder.Add(ShaderDocumentBlockKind::MainBody, "void main() {}\n")
     || builder.Add(ShaderDocumentBlockKind::Resource, "layout(set=0) uniform X {};\n")
     || builder_diagnostics.GetCount() != 1)
        return 11;

    ShaderLegacyAuditShell audit;
    audit.BeginAudit();
    audit.CompleteAudit();
    if (audit.GetState() != ShaderLegacyAuditState::ReadyForCleanup
     || !audit.IsCleanupSafe())
        return 12;

    return 0;
}
