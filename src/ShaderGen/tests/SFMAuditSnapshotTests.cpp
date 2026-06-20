#include <hgl/shadergen/ShaderResourceScanner.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

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

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))

static bool ReadTextFile(const char *path, std::string &out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return true;
}

static std::string BuildSnapshot(const SFMAnnotationScanReport &report)
{
    std::string out;

    for (const auto &record : report.records)
    {
        out += record.key;
        out += ':';
        for (size_t i = 0; i < record.args.size(); ++i)
        {
            if (i)
                out += ',';
            out += record.args[i];
        }
        out += '\n';
    }

    return out;
}

static void CheckSurfaceSnapshot(const char *path, const char *expected_snapshot)
{
    std::string source;
    CHECK_TRUE(ReadTextFile(path, source));
    if (source.empty())
        return;

    SFMAnnotationScanReport report;
    std::string diagnostics;
    CHECK_TRUE(ParseSFMAnnotationsFromGLSL(source, report, &diagnostics));
    CHECK_TRUE(report.issues.empty());

    const std::string snapshot = BuildSnapshot(report);
    CHECK_TRUE(snapshot == expected_snapshot);

    if (snapshot != expected_snapshot)
    {
        std::fprintf(stderr,
                     "Snapshot mismatch for %s\nExpected:\n%s\nActual:\n%s\n",
                     path,
                     expected_snapshot,
                     snapshot.c_str());
    }
}

static void test_core_surface_snapshots()
{
    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/billboard_texture_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward\n"
        "require:texture,base_color\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/unlit_texture3d_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward,shadow\n"
        "require:texture,base_color\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/unlit_color3d_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward,shadow\n"
        "require:ssbo,material_instance\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/standard_surface.glsl",
        "surface_type:standard\n"
        "supports_phase:forward\n"
        "require:texture,base_color,normal\n"
        "require:ssbo,material_instance\n"
        "require:ubo,camera,sky\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/sky_minimal_surface.glsl",
        "surface_type:sky\n"
        "supports_phase:forward\n"
        "require:ubo,sky\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/2d/puretexture2d_surface.glsl",
        "surface_type:puretexture2d\n"
        "supports_phase:forward\n"
        "require:texture,base_color\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/2d/text2d_surface.glsl",
        "surface_type:text2d\n"
        "supports_phase:forward\n"
        "require:texture,text\n"
        "require:ssbo,material_instance\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/unlit_vertexcolor_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward,shadow\n"
        "require:va,vertex_color\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/unlit_luminance_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward,shadow\n"
        "require:va,luminance\n"
        "require:ssbo,material_instance\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/gizmo3d_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward\n"
        "require:va,normal\n"
        "require:ssbo,material_instance\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/terrain_grid_surface.glsl",
        "surface_type:terrain\n"
        "supports_phase:forward\n"
        "require:ubo,camera\n"
        "require:va,normal\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/pbrcolor3d_surface.glsl",
        "surface_type:standard\n"
        "supports_phase:forward\n"
        "require:ssbo,material_instance\n"
        "require:ubo,sky\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/textureblinnphong_surface.glsl",
        "surface_type:standard\n"
        "supports_phase:forward\n"
        "require:texture,base_color,normal\n"
        "require:ssbo,material_instance\n");

    CheckSurfaceSnapshot(
        "ShaderLibrary/surface/error_indicator_surface.glsl",
        "surface_type:unlit\n"
        "supports_phase:forward\n");
}

int main()
{
    test_core_surface_snapshots();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All SFM audit snapshot tests passed.\n");
    return 0;
}
