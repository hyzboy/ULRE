#include <hgl/shadergen/ShaderResourceScanner.h>
#include <hgl/shadergen/registry/ErrorCodeRegistry.h>

#include <cstdio>
#include <string>

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))

using namespace hgl::graph::mtl;

static void test_accepts_valid_annotations()
{
    const std::string source =
        "// @sfm:surface_type unlit\n"
        "// @sfm:supports_phase forward deferred\n"
        "// @sfm:require texture base_color\n"
        "// @sfm:optional texture normal_map\n";

    SFMAnnotationScanReport report;
    std::string diagnostics;
    CHECK_TRUE(ParseSFMAnnotationsFromGLSL(source, report, &diagnostics));
    CHECK_TRUE(report.issues.empty());
    CHECK_EQ(report.records.size(), static_cast<size_t>(4));
}

static void test_rejects_duplicate_directives()
{
    const std::string source =
        "// @sfm:surface_type unlit\n"
        "// @sfm:surface_type lit\n";

    SFMAnnotationScanReport report;
    std::string diagnostics;
    CHECK_TRUE(!ParseSFMAnnotationsFromGLSL(source, report, &diagnostics));
    CHECK_TRUE(!report.issues.empty());

    const auto decoded = DecodeSFMAnnotationError(report.issues.front().error_code);
    CHECK_EQ(decoded.error, SFMAnnotationError::ConflictingKey);
}

static void test_rejects_unknown_key()
{
    const std::string source =
        "// @sfm:surface_type unlit\n"
        "// @sfm:foo bar\n";

    SFMAnnotationScanReport report;
    std::string diagnostics;
    CHECK_TRUE(!ParseSFMAnnotationsFromGLSL(source, report, &diagnostics));
    CHECK_TRUE(!report.issues.empty());

    const auto decoded = DecodeSFMAnnotationError(report.issues.front().error_code);
    CHECK_EQ(decoded.error, SFMAnnotationError::UnknownKey);
}

static void test_rejects_derive_out_of_range()
{
    const std::string source =
        "// @sfm:surface_type unlit\n"
        "// @sfm:derive texture emissive_map\n";

    SFMAnnotationScanReport report;
    std::string diagnostics;
    CHECK_TRUE(!ParseSFMAnnotationsFromGLSL(source, report, &diagnostics));
    CHECK_TRUE(!report.issues.empty());

    bool found = false;
    for (const auto &issue : report.issues)
    {
        const auto decoded = DecodeSFMAnnotationError(issue.error_code);
        if (decoded.error == SFMAnnotationError::DeriveOutOfRange)
            found = true;
    }
    CHECK_TRUE(found);
}

int main()
{
    test_accepts_valid_annotations();
    test_rejects_duplicate_directives();
    test_rejects_unknown_key();
    test_rejects_derive_out_of_range();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All SFMAnnotationScanner tests passed.\n");
    return 0;
}
