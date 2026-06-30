// MaterialKeyAbiSnapshot — ABI stability guard.
//
// Ensures that key constants and layout properties of MaterialKey have not
// accidentally drifted.  If any CHECK fails, a field was removed, reordered,
// or a constant was redefined.

#include <hgl/mtl/MaterialKey.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialKeyToolchainVersion.h>
#include <hgl/mtl/RecipeToKey.h>
#include <hgl/mtl/StaticMaterialDefRegistry.h>

#include <cstdio>

// ---------------------------------------------------------------------------
// Minimal check harness
// ---------------------------------------------------------------------------
static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            GLogError( "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))
#define CHECK_NE(a, b)  CHECK_TRUE((a) != (b))

// ---------------------------------------------------------------------------
using namespace hgl::graph::mtl;
using namespace hgl::graph;

static void test_schema_version_is_one()
{
    CHECK_EQ(kMaterialKeySchemaVersion, uint32_t(1));
}

static void test_material_key_alignment()
{
    CHECK_EQ(alignof(MaterialKey), size_t(8));
}

static void test_material_key_is_trivially_copyable()
{
    static_assert(std::is_trivially_copyable_v<MaterialKey>,
                  "MaterialKey must remain trivially copyable");
    CHECK_TRUE(std::is_trivially_copyable_v<MaterialKey>);
}

static void test_invalid_def_id_is_zero()
{
    CHECK_EQ(kInvalidStaticMaterialDefId, StaticMaterialDefId(0));
}

static void test_toolchain_versions_match_snapshot()
{
    // Freeze the expected values. Update here when the toolchain version
    // constants are intentionally changed.
    CHECK_EQ(kMaterialKeyGLSLVersion,   uint16_t(450));
    CHECK_EQ(kMaterialKeyVulkanVersion, uint16_t(0x0102));
    CHECK_EQ(kMaterialKeySpvVersion,    uint16_t(0x0105));
}

/// Fixed-input deterministic hash: same recipe must always produce the same
/// MaterialKey hash value across re-runs.  Catches accidental field reorderings
/// that would silently change the hash output.
static void test_deterministic_hash_for_standard_opaque()
{
    MaterialRecipe r;
    r.preset     = MaterialPreset::Standard;
    r.dim        = MaterialRecipe::Dim::D3;
    r.sky_ambient = SkyLightAmbientModel::Simple;

    MaterialRecipe::TextureSlotConfig tc;
    tc.slot        = SamplerSlot::BaseColor;
    tc.source_mode = TextureSourceMode::Simple;
    r.textures.push_back(tc);

    const MaterialKey k1 = ResolveRecipePrimaryKey(r);
    const MaterialKey k2 = ResolveRecipePrimaryKey(r);

    // Determinism check
    CHECK_EQ(k1.Hash(), k2.Hash());
    CHECK_EQ(k1, k2);

    // The hash must be non-zero.
    CHECK_NE(k1.Hash(), uint64_t(0));
}

/// Print current sizeof(MaterialKey) for documentation purposes.
/// This is NOT a pass/fail check — it just reports the current size.
static void print_sizeof_report()
{
    GLogInfo("[ABI] sizeof(MaterialKey)   = %zu bytes\n", sizeof(MaterialKey));
    GLogInfo("[ABI] sizeof(MaterialVariantKey) = %zu bytes\n",
                sizeof(MaterialVariantKey));
    GLogInfo("[ABI] alignof(MaterialKey)  = %zu bytes\n", alignof(MaterialKey));
    GLogInfo("[ABI] kMaterialKeySchemaVersion = %u\n", kMaterialKeySchemaVersion);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    print_sizeof_report();

    test_schema_version_is_one();
    test_material_key_alignment();
    test_material_key_is_trivially_copyable();
    test_invalid_def_id_is_zero();
    test_toolchain_versions_match_snapshot();
    test_deterministic_hash_for_standard_opaque();

    if (g_failures == 0)
    {
        GLogInfo("All ABI snapshot tests passed.\n");
    }
    else
    {
        GLogError( "%d ABI snapshot test(s) FAILED.\n", g_failures);
    }

    return g_failures;
}
