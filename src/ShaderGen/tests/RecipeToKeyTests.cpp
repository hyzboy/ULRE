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

static void test_sky_canonicalization_for_standard_mesh3d()
{
    MaterialRecipe r = MakeStandardOpaqueRecipe();
    r.sky_ambient = SkyLightAmbientModel::CubeMap;

    const MaterialKey k = ResolveRecipePrimaryKey(r);
    CHECK_EQ(k.variant.sky_ambient_model, SkyLightAmbientModel::Simple);
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

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All RecipeToKey tests passed.\n");
    return 0;
}
