#include <hgl/mtl/ShaderDocumentLegacyAdapter.h>
#include <hgl/log/Log.h>

using namespace hgl::graph::mtl;
using hgl::AnsiString;

namespace
{
    int FindFirstDifference(const AnsiString &lhs, const AnsiString &rhs)
    {
        const int common_length = lhs.Length() < rhs.Length()
            ? lhs.Length()
            : rhs.Length();
        for (int i = 0; i < common_length; ++i)
            if (lhs[i] != rhs[i])
                return i;
        return lhs.Length() == rhs.Length() ? -1 : common_length;
    }

    int GetLineNumber(const AnsiString &text, const int offset)
    {
        int line = 1;
        for (int i = 0; i < offset && i < text.Length(); ++i)
            if (text[i] == '\n')
                ++line;
        return line;
    }

    AnsiString GetContext(const AnsiString &text, const int offset)
    {
        const int begin = offset > 24 ? offset - 24 : 0;
        const int end = offset + 24 < text.Length()
            ? offset + 24
            : text.Length();
        return text.SubString(begin, end - begin);
    }
}

int main()
{
    const AnsiString legacy =
        "#version 460\n"
        "void main() {}\n";
    const AnsiString expected =
        "#version 460\n"
        "#define HGL_COMPARE 1\n"
        "void main() {}\n";

    ShaderDocument document;
    ShaderDocumentDiagnostics diagnostics;
    if (!BuildInjectedShaderDocument(
            legacy,
            "#define HGL_COMPARE 1\n",
            "fragment",
            document,
            diagnostics))
    {
        GLogError("[ShaderLegacyDocumentCompare] failed to build Document");
        return 1;
    }

    AnsiString serialized;
    if (!document.Serialize(serialized, diagnostics))
    {
        GLogError("[ShaderLegacyDocumentCompare] Document serialization failed");
        return 2;
    }

    const int difference = FindFirstDifference(expected, serialized);
    if (difference >= 0)
    {
        GLogError(
            "[ShaderLegacyDocumentCompare] first byte difference at offset %d "
            "line %d (legacy=%d document=%d), context=%s",
            difference,
            GetLineNumber(expected, difference),
            expected.Length(),
            serialized.Length(),
            GetContext(expected, difference).c_str());
        return 3;
    }

    GLogInfo(
        "[ShaderLegacyDocumentCompare] byte-identical; source hash=%llu",
        static_cast<unsigned long long>(document.GetSerializedHash(diagnostics)));
    return 0;
}
