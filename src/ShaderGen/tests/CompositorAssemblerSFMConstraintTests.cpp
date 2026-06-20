#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantRow.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

static bool Contains(const std::string &text, const char *needle)
{
    return text.find(needle) != std::string::npos;
}

static MaterialVariantKey MakeBaseKey()
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;
    key.pass_hint = PassType::ForwardOpaque;
    key.blend_mode = RenderAlphaMode::Opaque;
    return key;
}

static bool WriteTextFile(const std::filesystem::path &path, const std::string &content)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    out << content;
    return out.good();
}

static std::filesystem::path MakeTempShaderLibraryRoot()
{
    std::error_code ec;
    const auto root = std::filesystem::temp_directory_path(ec) / "ulre_shadergen_sfm_constraints";
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "surface", ec);
    return root;
}

static MaterialVariantDesc MakeRowBoundDesc(const MaterialVariantKey &key,
                                            const char *surface_path,
                                            MaterialVariantRow &row)
{
    row.name = "SFMConstraintTestRow";
    row.surface_type = key.surface_type;
    row.geometry_mode = key.geometry_mode;
    row.position_provider = key.position_provider;
    row.blend = key.blend_mode;
    row.pass = key.pass_hint;
    row.surface_path = surface_path;

    return MaterialVariantDesc::CreateRowBound(row.name, &row, std::nullopt, {}, {}, surface_path);
}

static void test_surface_type_mismatch_fails()
{
    const auto shader_root = MakeTempShaderLibraryRoot();

    CHECK_TRUE(WriteTextFile(shader_root / "surface" / "sfm_surface_type_mismatch.glsl",
        "// @sfm:surface_type standard\n"
        "// @sfm:supports_phase forward\n"
        "vec4 _dummy_sfm_mismatch = vec4(0.0);\n"));

    CompositorAssembler assembler(shader_root.string());

    MaterialVariantKey key = MakeBaseKey();
    key.surface_type = SurfaceType::Unlit;
    key.pass_hint = PassType::ForwardOpaque;

    MaterialVariantRow row{};
    MaterialVariantDesc desc = MakeRowBoundDesc(key, "surface/sfm_surface_type_mismatch.glsl", row);

    const auto result = assembler.AssembleFragmentShader(key, desc);
    CHECK_TRUE(!result.success);
    CHECK_TRUE(Contains(result.error_message, "VT-ERR-SFM-SURFACE-MISMATCH"));
}

static void test_phase_mismatch_fails()
{
    const auto shader_root = MakeTempShaderLibraryRoot();

    CHECK_TRUE(WriteTextFile(shader_root / "surface" / "sfm_phase_mismatch.glsl",
        "// @sfm:surface_type unlit\n"
        "// @sfm:supports_phase shadow\n"
        "vec4 _dummy_sfm_phase = vec4(0.0);\n"));

    CompositorAssembler assembler(shader_root.string());

    MaterialVariantKey key = MakeBaseKey();
    key.surface_type = SurfaceType::Unlit;
    key.pass_hint = PassType::ForwardOpaque;

    MaterialVariantRow row{};
    MaterialVariantDesc desc = MakeRowBoundDesc(key, "surface/sfm_phase_mismatch.glsl", row);

    const auto result = assembler.AssembleFragmentShader(key, desc);
    CHECK_TRUE(!result.success);
    CHECK_TRUE(Contains(result.error_message, "VT-ERR-SFM-PHASE-UNSUPPORTED"));
}

static void test_invalid_sfm_annotation_fails()
{
    const auto shader_root = MakeTempShaderLibraryRoot();

    CHECK_TRUE(WriteTextFile(shader_root / "surface" / "sfm_invalid.glsl",
        "// @sfm:unknown_key value\n"
        "vec4 _dummy_sfm_invalid = vec4(0.0);\n"));

    CompositorAssembler assembler(shader_root.string());

    MaterialVariantKey key = MakeBaseKey();
    MaterialVariantRow row{};
    MaterialVariantDesc desc = MakeRowBoundDesc(key, "surface/sfm_invalid.glsl", row);

    const auto result = assembler.AssembleFragmentShader(key, desc);
    CHECK_TRUE(!result.success);
    CHECK_TRUE(Contains(result.error_message, "VT-ERR-SFM-PARSE"));
}

static void test_missing_sfm_annotations_allowed()
{
    const auto shader_root = MakeTempShaderLibraryRoot();

    CHECK_TRUE(WriteTextFile(shader_root / "surface" / "sfm_empty.glsl",
        "vec4 _dummy_sfm_empty = vec4(0.0);\n"));

    CompositorAssembler assembler(shader_root.string());

    MaterialVariantKey key = MakeBaseKey();
    MaterialVariantRow row{};
    MaterialVariantDesc desc = MakeRowBoundDesc(key, "surface/sfm_empty.glsl", row);

    const auto result = assembler.AssembleFragmentShader(key, desc);
    CHECK_TRUE(result.success);
}

int main()
{
    test_surface_type_mismatch_fails();
    test_phase_mismatch_fails();
    test_invalid_sfm_annotation_fails();
    test_missing_sfm_annotations_allowed();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All CompositorAssemblerSFMConstraint tests passed.\n");
    return 0;
}
