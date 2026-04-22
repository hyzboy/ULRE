#include <hgl/shadergen/internal/GLSLSourceUtils.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

using namespace hgl::graph::internal;

static int s_failures = 0;

static void Check(const char *test_name, bool condition)
{
    if (condition)
    {
        std::fprintf(stdout, "  PASS: %s\n", test_name);
    }
    else
    {
        std::fprintf(stderr, "  FAIL: %s\n", test_name);
        ++s_failures;
    }
}

// ---------------------------------------------------------------------------
// FindVersionDirectiveLineEnd
// ---------------------------------------------------------------------------

static void Test_FindVersion_Basic()
{
    const std::string src = "#version 450\nvoid main(){}\n";
    const size_t pos = FindVersionDirectiveLineEnd(src);
    // Should point past the '\n' after "450"
    Check("FindVersion_Basic: returns offset past #version newline",
          pos == 13 && src[pos] == 'v'); // "void" starts here
}

static void Test_FindVersion_BOM()
{
    // UTF-8 BOM prefix
    const std::string src = "\xEF\xBB\xBF#version 450\nvoid main(){}\n";
    const size_t pos = FindVersionDirectiveLineEnd(src);
    Check("FindVersion_BOM: skips BOM correctly",
          pos != std::string::npos && src.substr(pos, 4) == "void");
}

static void Test_FindVersion_NoVersion()
{
    const std::string src = "void main(){}\n";
    Check("FindVersion_NoVersion: returns npos when no #version",
          FindVersionDirectiveLineEnd(src) == std::string::npos);
}

static void Test_FindVersion_NoNewline()
{
    const std::string src = "#version 450";
    const size_t pos = FindVersionDirectiveLineEnd(src);
    // No '\n' → returns source.size()
    Check("FindVersion_NoNewline: returns source.size() when no trailing newline",
          pos == src.size());
}

// ---------------------------------------------------------------------------
// InjectAfterVersion
// ---------------------------------------------------------------------------

static void Test_InjectAfterVersion_WithVersion()
{
    const std::string src = "#version 450\nvoid main(){}\n";
    const std::string inj = "#define FOO 1";
    const std::string result = InjectAfterVersion(src, inj);
    // Expected: "#version 450\n\n#define FOO 1\nvoid main(){}\n"
    Check("InjectAfterVersion_WithVersion: injection appears after #version line",
          result.find("#define FOO 1") != std::string::npos &&
          result.find("#version 450") < result.find("#define FOO 1") &&
          result.find("#define FOO 1") < result.find("void main"));
}

static void Test_InjectAfterVersion_WithoutVersion()
{
    const std::string src = "void main(){}\n";
    const std::string inj = "#define BAR 2";
    const std::string result = InjectAfterVersion(src, inj);
    // Expected: "#define BAR 2\nvoid main(){}\n"
    Check("InjectAfterVersion_WithoutVersion: injection prepended when no #version",
          result.find("#define BAR 2") == 0 &&
          result.find("void main") > result.find("#define BAR 2"));
}

static void Test_InjectAfterVersion_EmptyInject()
{
    const std::string src = "#version 450\nvoid main(){}\n";
    const std::string result = InjectAfterVersion(src, std::string());
    Check("InjectAfterVersion_EmptyInject: source unchanged when injection empty",
          result == src);
}

static void Test_InjectAfterVersion_BOMSource()
{
    const std::string src = "\xEF\xBB\xBF#version 450\nvoid main(){}\n";
    const std::string inj = "#define BOM_OK 1";
    const std::string result = InjectAfterVersion(src, inj);
    Check("InjectAfterVersion_BOMSource: injection after BOM #version",
          result.find("#define BOM_OK 1") != std::string::npos &&
          result.find("#version") < result.find("#define BOM_OK 1") &&
          result.find("#define BOM_OK 1") < result.find("void main"));
}

// ---------------------------------------------------------------------------
// BuildGLSLPreviewFirstLines
// ---------------------------------------------------------------------------

static void Test_BuildGLSLPreview_Truncate()
{
    const std::string src = "line1\nline2\nline3\nline4\nline5\n";
    const std::string result = BuildGLSLPreviewFirstLines(src, 3);
    Check("BuildGLSLPreview_Truncate: returns only first 3 lines",
          result == "line1\nline2\nline3\n");
}

static void Test_BuildGLSLPreview_LessThanMax()
{
    const std::string src = "line1\nline2\n";
    const std::string result = BuildGLSLPreviewFirstLines(src, 10);
    Check("BuildGLSLPreview_LessThanMax: returns full source when fewer lines than max",
          result == src);
}

// ---------------------------------------------------------------------------
// ReadTextFile
// ---------------------------------------------------------------------------

static void Test_ReadTextFile_Missing()
{
    std::string content, error;
    const bool ok = ReadTextFile("/nonexistent/path/to/file.txt", content, error);
    Check("ReadTextFile_Missing: returns false on missing file",
          !ok && !error.empty());
}

static void Test_ReadTextFile_OK()
{
    // Write a temp file then read it back
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "glsl_source_utils_test_tmp.txt";
    {
        FILE *f = std::fopen(tmp.string().c_str(), "w");
        if (!f) { Check("ReadTextFile_OK: (could not create temp file)", false); return; }
        std::fputs("#version 450\nvoid main(){}\n", f);
        std::fclose(f);
    }

    std::string content, error;
    const bool ok = ReadTextFile(tmp.string(), content, error);
    fs::remove(tmp);

    Check("ReadTextFile_OK: reads file content correctly",
          ok && content == "#version 450\nvoid main(){}\n" && error.empty());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::fprintf(stdout, "[GLSLSourceUtilsTest] Start\n");

    Test_FindVersion_Basic();
    Test_FindVersion_BOM();
    Test_FindVersion_NoVersion();
    Test_FindVersion_NoNewline();

    Test_InjectAfterVersion_WithVersion();
    Test_InjectAfterVersion_WithoutVersion();
    Test_InjectAfterVersion_EmptyInject();
    Test_InjectAfterVersion_BOMSource();

    Test_BuildGLSLPreview_Truncate();
    Test_BuildGLSLPreview_LessThanMax();

    Test_ReadTextFile_Missing();
    Test_ReadTextFile_OK();

    if (s_failures == 0)
    {
        std::fprintf(stdout, "[GLSLSourceUtilsTest] ALL PASS\n");
        return 0;
    }
    else
    {
        std::fprintf(stderr, "[GLSLSourceUtilsTest] %d FAILURE(S)\n", s_failures);
        return 1;
    }
}
