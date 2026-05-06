// CompositorAssembler golden-style tests.
//
// Focus:
//  1) Key-derived define injection in generated VS/FS GLSL.
//  2) Surface include path propagation.
//  3) Non-compositor template path read-failure diagnostics.

#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantKey.h>

#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Minimal check harness
// ---------------------------------------------------------------------------
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

using namespace hgl::graph;
using namespace hgl::graph::mtl;

static bool Contains(const std::string &text, const std::string &needle)
{
    return text.find(needle) != std::string::npos;
}

static void test_generated_sources_inject_key_defines_and_surface_include()
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Array);

    MaterialVariantDesc desc;
    desc.surface_function_path = "surface/golden_surface.glsl";

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(key, desc);

    CHECK_TRUE(result.success);
    if (!result.success)
        return;

    const std::string surface_define =
        std::string("#define SURFACE_TYPE ") + std::to_string(static_cast<int>(key.surface_type));

    CHECK_TRUE(Contains(result.vertex_glsl, "#version "));
    CHECK_TRUE(Contains(result.fragment_glsl, "#version "));

    CHECK_TRUE(Contains(result.vertex_glsl, surface_define));
    CHECK_TRUE(Contains(result.fragment_glsl, surface_define));
    CHECK_TRUE(Contains(result.vertex_glsl, "#define SHADOW_MODE 0"));
    CHECK_TRUE(Contains(result.fragment_glsl, "#define SHADOW_MODE 0"));

    CHECK_TRUE(Contains(result.vertex_glsl, "#define TEXTURE_ARRAY_MODE"));
    CHECK_TRUE(Contains(result.fragment_glsl, "#define TEXTURE_ARRAY_MODE"));
    CHECK_TRUE(Contains(result.fragment_glsl, "#include \"surface/golden_surface.glsl\""));

    const size_t version_pos = result.fragment_glsl.find("#version ");
    const size_t define_pos = result.fragment_glsl.find("#define SURFACE_TYPE ");
    CHECK_TRUE(version_pos != std::string::npos);
    CHECK_TRUE(define_pos != std::string::npos);
    if (version_pos != std::string::npos && define_pos != std::string::npos)
        CHECK_TRUE(version_pos < define_pos);
}

static void test_non_array_texture_mode_does_not_inject_array_define()
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);

    MaterialVariantDesc desc;
    desc.surface_function_path = "surface/golden_surface.glsl";

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(key, desc);

    CHECK_TRUE(result.success);
    if (!result.success)
        return;

    CHECK_TRUE(!Contains(result.vertex_glsl, "#define TEXTURE_ARRAY_MODE"));
    CHECK_TRUE(!Contains(result.fragment_glsl, "#define TEXTURE_ARRAY_MODE"));
}

static void test_non_compositor_template_path_reports_read_failure()
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;

    MaterialVariantDesc desc;
    desc.vs_template_path = "tests/nonexistent_custom_template.vert.glsl";
    desc.surface_function_path = "surface/golden_surface.glsl";

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(key, desc);

    CHECK_TRUE(!result.success);
    if (result.success)
        return;

    CHECK_TRUE(Contains(result.error_message, "template load failed"));
    CHECK_TRUE(Contains(result.error_message, desc.vs_template_path));
}

int main()
{
    test_generated_sources_inject_key_defines_and_surface_include();
    test_non_array_texture_mode_does_not_inject_array_define();
    test_non_compositor_template_path_reports_read_failure();

    if (g_failures == 0)
        std::printf("All tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
