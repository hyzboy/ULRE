// MaterialFactory3D registration tests - standalone executable.
//
// Verifies explicit builtin registration entry behaviour:
// 1) RegisterBuiltinFactories() registers all builtin presets.
// 2) Registered names match canonical preset token names.
// 3) RegisterBuiltinFactories() is idempotent.

#include <hgl/shadergen/MaterialFactory3D.h>

#include <array>
#include <cstdio>
#include <cstring>

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

struct BuiltinPresetExpectation
{
    MaterialPreset preset;
    const char    *name;
};

static constexpr std::array<BuiltinPresetExpectation, 18> kBuiltinExpectations = {{
    {MaterialPreset::PureColor2D,        "PureColor2D"},
    {MaterialPreset::PureTexture2D,      "PureTexture2D"},
    {MaterialPreset::VertexColor2D,      "VertexColor2D"},
    {MaterialPreset::Text2D,             "Text2D"},
    {MaterialPreset::Checkerboard3D,     "Checkerboard3D"},
    {MaterialPreset::FullscreenTriangle, "FullscreenTriangle"},
    {MaterialPreset::PureColor3D,        "PureColor3D"},
    {MaterialPreset::VertexColor3D,      "VertexColor3D"},
    {MaterialPreset::VertexLuminance3D,  "VertexLuminance3D"},
    {MaterialPreset::VertexLuminance2D,  "VertexLuminance2D"},
    {MaterialPreset::Billboard2DDynamic, "Billboard2DDynamic"},
    {MaterialPreset::Billboard2DFixed,   "Billboard2DFixed"},
    {MaterialPreset::Gizmo3D,            "Gizmo3D"},
    {MaterialPreset::SkyMinimal,         "SkyMinimal"},
    {MaterialPreset::Standard,           "Standard"},
    {MaterialPreset::PBRColor3D,         "PBRColor3D"},
    {MaterialPreset::VertexPaletteColor3D,"VertexPaletteColor3D"},
    {MaterialPreset::TerrainGrid,        "TerrainGrid"},
}};

static void test_builtin_presets_are_registered_with_expected_names()
{
    hgl::graph::mtl::MaterialFactory3D::RegisterBuiltinFactories();

    for (const auto &it : kBuiltinExpectations)
    {
        const char *registered_name = hgl::graph::mtl::MaterialFactory3D::GetRegisteredName(it.preset);

        CHECK_TRUE(registered_name != nullptr);
        if (!registered_name) continue;

        CHECK_TRUE(registered_name[0] != '\0');
        CHECK_TRUE(std::strcmp(registered_name, it.name) == 0);
    }

    CHECK_TRUE(hgl::graph::mtl::MaterialFactory3D::RegisteredCount() >= kBuiltinExpectations.size());
}

static void test_register_builtin_factories_is_idempotent()
{
    hgl::graph::mtl::MaterialFactory3D::RegisterBuiltinFactories();
    const size_t before = hgl::graph::mtl::MaterialFactory3D::RegisteredCount();

    hgl::graph::mtl::MaterialFactory3D::RegisterBuiltinFactories();
    const size_t after = hgl::graph::mtl::MaterialFactory3D::RegisteredCount();

    CHECK_EQ(before, after);
}

int main()
{
    test_builtin_presets_are_registered_with_expected_names();
    test_register_builtin_factories_is_idempotent();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All MaterialFactory3D registration tests passed.\n");
    return 0;
}
