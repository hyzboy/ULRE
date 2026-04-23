// EnumerateRecipeKeys unit tests — standalone executable, no external test framework.
//
// Tests that EnumerateRecipeKeys returns the correct pass-expanded keys for
// various blend modes, and that all non-pass fields are identical across the
// returned vector.

#include <hgl/mtl/RecipeToKey.h>
#include <hgl/mtl/StaticMaterialDefRegistry.h>
#include <hgl/mtl/MaterialKeyToolchainVersion.h>

#include <cstdio>
#include <vector>

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
#define CHECK_NE(a, b)  CHECK_TRUE((a) != (b))

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
using namespace hgl::graph::mtl;
using namespace hgl::graph;

// Billboard recipes allow per-recipe blend mode override.
static MaterialRecipe MakeBillboardRecipe(RenderAlphaMode blend)
{
    MaterialRecipe r;
    r.preset = MaterialPreset::Billboard2DDynamic;
    r.billboard.blend_mode = blend;
    r.billboard.texture_id = "test";
    return r;
}

// Standard (3-D, non-billboard) recipe — blend mode is always Opaque.
static MaterialRecipe MakeStandardRecipe()
{
    MaterialRecipe r;
    r.preset = MaterialPreset::Standard;
    r.dim    = MaterialRecipe::Dim::D3;
    r.sky_ambient = SkyLightAmbientModel::Simple;

    MaterialRecipe::TextureSlotConfig tc;
    tc.slot        = SamplerSlot::BaseColor;
    tc.source_mode = TextureSourceMode::Simple;
    r.textures.push_back(tc);

    return r;
}

static MaterialRecipe MakeStandardArrayRecipe()
{
    MaterialRecipe r;
    r.preset = MaterialPreset::Standard;
    r.dim    = MaterialRecipe::Dim::D3;
    r.sky_ambient = SkyLightAmbientModel::Simple;

    MaterialRecipe::TextureSlotConfig tc;
    tc.slot        = SamplerSlot::BaseColor;
    tc.source_mode = TextureSourceMode::Array;
    r.textures.push_back(tc);

    return r;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/// Opaque blend → 3 passes (ForwardOpaque, ShadowOpaque, EarlyZSolid)
static void test_opaque_produces_three_passes()
{
    const auto keys = EnumerateRecipeKeys(MakeBillboardRecipe(RenderAlphaMode::Opaque));
    CHECK_EQ(keys.size(), size_t(3));
}

/// Transparent blend → 1 pass (ForwardTransparent)
static void test_transparent_produces_one_pass()
{
    const auto keys = EnumerateRecipeKeys(MakeBillboardRecipe(RenderAlphaMode::Transparent));
    CHECK_EQ(keys.size(), size_t(1));
}

/// Masked blend → 3 passes (ForwardMasked, ShadowMasked, EarlyZMasked)
static void test_masked_produces_three_passes()
{
    const auto keys = EnumerateRecipeKeys(MakeBillboardRecipe(RenderAlphaMode::Masked));
    CHECK_EQ(keys.size(), size_t(3));
}

/// All keys returned for opaque must have distinct pass fields.
static void test_passes_are_distinct()
{
    const auto keys = EnumerateRecipeKeys(MakeBillboardRecipe(RenderAlphaMode::Opaque));
    CHECK_EQ(keys.size(), size_t(3));
    if (keys.size() < 3) return; // avoid OOB if count wrong
    CHECK_NE(keys[0].pass, keys[1].pass);
    CHECK_NE(keys[0].pass, keys[2].pass);
    CHECK_NE(keys[1].pass, keys[2].pass);
}

/// All non-pass fields must be identical across the returned keys.
static void test_non_pass_fields_are_identical()
{
    const auto keys = EnumerateRecipeKeys(MakeBillboardRecipe(RenderAlphaMode::Opaque));
    CHECK_TRUE(!keys.empty());
    if (keys.empty()) return;

    const MaterialKey &first = keys[0];
    for (size_t i = 1; i < keys.size(); ++i)
    {
        const MaterialKey &k = keys[i];
        CHECK_EQ(k.variant.Hash(), first.variant.Hash());
        CHECK_EQ(k.def_id,         first.def_id);
        CHECK_EQ(k.schema,         first.schema);
        CHECK_EQ(k.glsl_version,   first.glsl_version);
        CHECK_EQ(k.vk_version,     first.vk_version);
        CHECK_EQ(k.spv_version,    first.spv_version);
    }
}

/// Standard preset → def_id must be non-zero.
static void test_standard_def_id_is_valid()
{
    const auto keys = EnumerateRecipeKeys(MakeStandardRecipe());
    CHECK_TRUE(!keys.empty());
    if (keys.empty()) return;

    CHECK_NE(keys[0].def_id, kInvalidStaticMaterialDefId);
}

/// Toolchain version fields must be non-zero.
static void test_toolchain_versions_are_nonzero()
{
    const auto keys = EnumerateRecipeKeys(MakeStandardRecipe());
    CHECK_TRUE(!keys.empty());
    if (keys.empty()) return;

    const MaterialKey &k = keys[0];
    CHECK_NE(k.glsl_version, uint16_t(0));
    CHECK_NE(k.vk_version,   uint16_t(0));
    CHECK_NE(k.spv_version,  uint16_t(0));
}

/// Standard def_id for array and non-array textures must differ.
static void test_array_and_flat_def_ids_differ()
{
    const auto keys_flat  = EnumerateRecipeKeys(MakeStandardRecipe());
    const auto keys_array = EnumerateRecipeKeys(MakeStandardArrayRecipe());

    CHECK_TRUE(!keys_flat.empty());
    CHECK_TRUE(!keys_array.empty());
    if (keys_flat.empty() || keys_array.empty()) return;

    CHECK_NE(keys_flat[0].def_id, keys_array[0].def_id);
}

/// Toolchain version constants have expected values.
static void test_toolchain_version_constants()
{
    CHECK_EQ(kMaterialKeyGLSLVersion,   uint16_t(450));
    CHECK_EQ(kMaterialKeyVulkanVersion, uint16_t(0x0102));
    CHECK_EQ(kMaterialKeySpvVersion,    uint16_t(0x0105));
    CHECK_EQ(kMaterialKeySchemaVersion, uint32_t(1));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_opaque_produces_three_passes();
    test_transparent_produces_one_pass();
    test_masked_produces_three_passes();
    test_passes_are_distinct();
    test_non_pass_fields_are_identical();
    test_standard_def_id_is_valid();
    test_toolchain_versions_are_nonzero();
    test_array_and_flat_def_ids_differ();
    test_toolchain_version_constants();

    if (g_failures == 0)
        std::printf("All tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
