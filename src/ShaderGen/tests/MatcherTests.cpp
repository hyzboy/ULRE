#include <hgl/shadergen/Matcher.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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
    const auto root = std::filesystem::temp_directory_path(ec) / "ulre_shadergen_matcher_tests";
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "surface", ec);
    return root;
}

static void test_matcher_hits_supported_candidate()
{
    const auto shader_root = MakeTempShaderLibraryRoot();

    CHECK_TRUE(WriteTextFile(shader_root / "surface" / "matcher_hit.glsl",
        "// @sfm:surface_type unlit\n"
        "// @sfm:supports_phase forward\n"
        "// @sfm:require va vertex_color\n"
        "vec4 _dummy = vec4(0.0);\n"));

    MaterialPresetTable table;
    MaterialPresetCandidate candidate;
    candidate.surface_path = "surface/matcher_hit.glsl";
    candidate.quality_level = MaterialLOD::Base;
    candidate.render_phase = RenderPhase::Forward;
    CHECK_TRUE(table.AddCandidate(MaterialPreset::VertexColor3D, candidate));

    MatcherResolveRequest req;
    req.preset_table = &table;
    const std::string shader_root_str = shader_root.string();
    req.shader_library_path = shader_root_str.c_str();
    req.preset = MaterialPreset::VertexColor3D;
    req.requested_quality = MaterialLOD::Base;
    req.phase = RenderPhase::Forward;
    req.surface_type = hgl::graph::SurfaceType::Unlit;
    req.capabilities.vertex_attribs.insert("vertex_color");

    const auto result = Matcher::Resolve(req);
    CHECK_TRUE(result.matched);
    CHECK_TRUE(!result.used_fallback);
    CHECK_TRUE(result.surface_path != nullptr);
}

static void test_matcher_fallback_on_missing_requirement()
{
    const auto shader_root = MakeTempShaderLibraryRoot();

    CHECK_TRUE(WriteTextFile(shader_root / "surface" / "matcher_miss.glsl",
        "// @sfm:surface_type unlit\n"
        "// @sfm:supports_phase forward\n"
        "// @sfm:require va vertex_color\n"
        "vec4 _dummy = vec4(0.0);\n"));

    MaterialPresetTable table;
    MaterialPresetCandidate candidate;
    candidate.surface_path = "surface/matcher_miss.glsl";
    candidate.quality_level = MaterialLOD::Base;
    candidate.render_phase = RenderPhase::Forward;
    CHECK_TRUE(table.AddCandidate(MaterialPreset::VertexColor3D, candidate));

    MatcherResolveRequest req;
    req.preset_table = &table;
    const std::string shader_root_str = shader_root.string();
    req.shader_library_path = shader_root_str.c_str();
    req.preset = MaterialPreset::VertexColor3D;
    req.requested_quality = MaterialLOD::Base;
    req.phase = RenderPhase::Forward;
    req.surface_type = hgl::graph::SurfaceType::Unlit;

    const auto result = Matcher::Resolve(req);
    CHECK_TRUE(!result.matched);
    CHECK_TRUE(result.used_fallback);
    CHECK_TRUE(result.failure_reason != nullptr);
    CHECK_TRUE(std::string(result.failure_reason) == "MT-FALLBACK-NO-MATCH");
}

static void test_matcher_fallback_without_preset_table()
{
    MatcherResolveRequest req;
    req.preset_table = nullptr;
    req.shader_library_path = "ignored";
    req.preset = MaterialPreset::VertexColor3D;

    const auto result = Matcher::Resolve(req);
    CHECK_TRUE(!result.matched);
    CHECK_TRUE(result.used_fallback);
    CHECK_TRUE(result.failure_reason != nullptr);
    CHECK_TRUE(std::string(result.failure_reason) == "MT-FALLBACK-NO-PRESET-TABLE");
}

static void test_matcher_fallback_without_shader_library_path()
{
    MaterialPresetTable table;
    MaterialPresetCandidate candidate;
    candidate.surface_path = "surface/unused.glsl";
    candidate.quality_level = MaterialLOD::Base;
    candidate.render_phase = RenderPhase::Forward;
    CHECK_TRUE(table.AddCandidate(MaterialPreset::VertexColor3D, candidate));

    MatcherResolveRequest req;
    req.preset_table = &table;
    req.shader_library_path = nullptr;
    req.preset = MaterialPreset::VertexColor3D;

    const auto result = Matcher::Resolve(req);
    CHECK_TRUE(!result.matched);
    CHECK_TRUE(result.used_fallback);
    CHECK_TRUE(result.failure_reason != nullptr);
    CHECK_TRUE(std::string(result.failure_reason) == "MT-FALLBACK-NO-SHADER-LIB");
}

static void test_matcher_fallback_when_no_candidate()
{
    MaterialPresetTable table;

    MatcherResolveRequest req;
    req.preset_table = &table;
    req.shader_library_path = "ignored";
    req.preset = MaterialPreset::VertexColor3D;
    req.phase = RenderPhase::Forward;
    req.requested_quality = MaterialLOD::Base;

    const auto result = Matcher::Resolve(req);
    CHECK_TRUE(!result.matched);
    CHECK_TRUE(result.used_fallback);
    CHECK_TRUE(result.failure_reason != nullptr);
    CHECK_TRUE(std::string(result.failure_reason) == "MT-FALLBACK-NO-CANDIDATE");
}

int main()
{
    test_matcher_hits_supported_candidate();
    test_matcher_fallback_on_missing_requirement();
    test_matcher_fallback_without_preset_table();
    test_matcher_fallback_without_shader_library_path();
    test_matcher_fallback_when_no_candidate();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All Matcher tests passed.\n");
    return 0;
}
