// RecipeToKey unit tests - standalone executable, no external test framework.
//
// Failure: any CHECK_* macro prints a message and increments g_failures.
// Success: main() returns 0.

#include <hgl/mtl/RecipeToKey.h>

#include <cstdio>

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

static MaterialRecipe MakeStandardOpaqueRecipe()
{
    MaterialRecipe r;
    r.preset = MaterialPreset::Standard;
    r.dim = MaterialRecipe::Dim::D3;
    r.sky_ambient = SkyLightAmbientModel::Simple;

    MaterialRecipe::TextureSlotConfig tc;
    tc.slot = SamplerSlot::BaseColor;
    tc.source_mode = TextureSourceMode::Simple;
    r.textures.push_back(tc);

    return r;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_deterministic_across_calls()
{
    const MaterialRecipe r = MakeStandardOpaqueRecipe();
    const MaterialKey k1 = ResolveRecipePrimaryKey(r);
    const MaterialKey k2 = ResolveRecipePrimaryKey(r);

    CHECK_EQ(k1, k2);
    CHECK_EQ(k1.Hash(), k2.Hash());
}

static void test_equivalent_recipes_produce_same_key()
{
    MaterialRecipe r1 = MakeStandardOpaqueRecipe();
    MaterialRecipe r2 = MakeStandardOpaqueRecipe();

    r2.id = "different_name";
    r2.domain_id = "different_domain";

    CHECK_EQ(ResolveRecipePrimaryKey(r1).Hash(), ResolveRecipePrimaryKey(r2).Hash());
}

static void test_texture_mode_change_affects_key()
{
    MaterialRecipe r1 = MakeStandardOpaqueRecipe();
    MaterialRecipe r2 = r1;

    r2.textures[0].source_mode = TextureSourceMode::Array;

    CHECK_NE(ResolveRecipePrimaryKey(r1).Hash(), ResolveRecipePrimaryKey(r2).Hash());
}

// Phase 6 regression: Standard + Mesh3D always canonicalizes sky to Simple,
// regardless of the authored sky_ambient value.
static void test_sky_canonicalization_for_standard_mesh3d()
{
    MaterialRecipe r = MakeStandardOpaqueRecipe();
    r.sky_ambient = SkyLightAmbientModel::CubeMap;

    const MaterialKey k = ResolveRecipePrimaryKey(r);
    CHECK_EQ(k.variant.sky_ambient_model, SkyLightAmbientModel::Simple);
}

// Phase 6 regression: Standard + FakeAtmosphere sky still canonicalizes to Simple,
// because Standard rows do not declare sky_is_routing_axis.
static void test_sky_canonicalization_standard_fakeatmosphere()
{
    MaterialRecipe r = MakeStandardOpaqueRecipe();
    r.sky_ambient = SkyLightAmbientModel::FakeAtmosphere;

    const MaterialKey k = ResolveRecipePrimaryKey(r);
    CHECK_EQ(k.variant.sky_ambient_model, SkyLightAmbientModel::Simple);
}

// Phase 6 regression: Standard + Simple sky must be idempotent under canonicalization.
static void test_sky_canonicalization_standard_simple_idempotent()
{
    MaterialRecipe r = MakeStandardOpaqueRecipe();
    r.sky_ambient = SkyLightAmbientModel::Simple;

    const MaterialKey k = ResolveRecipePrimaryKey(r);
    CHECK_EQ(k.variant.sky_ambient_model, SkyLightAmbientModel::Simple);
}

// Phase 6 regression: StandardPBRArray + FakeAtmosphere also collapses to Simple.
static void test_sky_canonicalization_stdpbrarray_fakeatmosphere()
{
    MaterialRecipe r;
    r.preset      = MaterialPreset::Standard;
    r.dim         = MaterialRecipe::Dim::D3;
    r.sky_ambient = SkyLightAmbientModel::FakeAtmosphere;

    MaterialRecipe::TextureSlotConfig tc;
    tc.slot        = SamplerSlot::BaseColor;
    tc.source_mode = TextureSourceMode::Array;
    r.textures.push_back(tc);

    const MaterialKey k = ResolveRecipePrimaryKey(r);
    CHECK_EQ(k.variant.sky_ambient_model, SkyLightAmbientModel::Simple);
}

// Phase 6 regression: two recipes that differ only in sky_ambient produce the
// same key when both are non-routing-axis presets.
static void test_different_sky_values_same_key_when_not_routing_axis()
{
    MaterialRecipe r_simple = MakeStandardOpaqueRecipe();
    r_simple.sky_ambient = SkyLightAmbientModel::Simple;

    MaterialRecipe r_fake = MakeStandardOpaqueRecipe();
    r_fake.sky_ambient = SkyLightAmbientModel::FakeAtmosphere;

    CHECK_EQ(ResolveRecipePrimaryKey(r_simple).Hash(),
             ResolveRecipePrimaryKey(r_fake).Hash());
}

static void test_explicit_schema_override_applies()
{
    MaterialRecipe r = MakeStandardOpaqueRecipe();
    r.has_explicit_schema = true;
    r.schema = ShaderDataSchema::PBRColorParams;

    const MaterialKey k = ResolveRecipePrimaryKey(r);
    CHECK_EQ(k.schema, ShaderDataSchema::PBRColorParams);
}

static void test_legacy_surface_model_field_compatibility()
{
    MaterialRecipe legacy;
    legacy.preset = MaterialPreset::Standard;
    legacy.surface_model = SurfaceShadingModel::StandardPBR;

    MaterialRecipe modern = legacy;
    modern.shading_model = modern.surface_model;

    CHECK_EQ(ResolveRecipePrimaryKey(legacy).Hash(),
             ResolveRecipePrimaryKey(modern).Hash());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_deterministic_across_calls();
    test_equivalent_recipes_produce_same_key();
    test_texture_mode_change_affects_key();
    test_sky_canonicalization_for_standard_mesh3d();
    test_sky_canonicalization_standard_fakeatmosphere();
    test_sky_canonicalization_standard_simple_idempotent();
    test_sky_canonicalization_stdpbrarray_fakeatmosphere();
    test_different_sky_values_same_key_when_not_routing_axis();
    test_explicit_schema_override_applies();
    test_legacy_surface_model_field_compatibility();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All RecipeToKey tests passed.\n");
    return 0;
}
