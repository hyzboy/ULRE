// MaterialKey unit tests — standalone executable, no external test framework.
//
// Failure: any CHECK_* macro prints a message and returns a non-zero exit code.
// Success: main() returns 0.

#include <hgl/mtl/MaterialKey.h>

#include <cstdio>
#include <cstdlib>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Minimal check harness
// ---------------------------------------------------------------------------
static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                     \
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

static MaterialKey default_key() { return MaterialKey{}; }

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Two default-constructed keys must be equal and produce the same hash.
static void test_default_construction_stable()
{
    const MaterialKey a = default_key();
    const MaterialKey b = default_key();
    CHECK_EQ(a, b);
    CHECK_EQ(a.Hash(), b.Hash());
    CHECK_EQ(a <=> b, std::strong_ordering::equal);
}

// Changing any tracked field must produce a different hash.
static void test_field_changes_affect_hash()
{
    const MaterialKey base = default_key();

    {
        MaterialKey k = base;
        k.variant.variant_row_name_hash = 1;
        CHECK_NE(k.Hash(), base.Hash());
        CHECK_NE(k, base);
    }
    {
        MaterialKey k = base;
        k.pass = PassType::ShadowOpaque;
        CHECK_NE(k.Hash(), base.Hash());
        CHECK_NE(k, base);
    }
    {
        MaterialKey k = base;
        k.def_id = 1;
        CHECK_NE(k.Hash(), base.Hash());
    }
    {
        MaterialKey k = base;
        k.schema = ShaderDataSchema::Color4f;
        CHECK_NE(k.Hash(), base.Hash());
    }
    {
        MaterialKey k = base;
        k.glsl_version = 460;
        CHECK_NE(k.Hash(), base.Hash());
    }
    {
        MaterialKey k = base;
        k.vk_version = 1;
        CHECK_NE(k.Hash(), base.Hash());
    }
    {
        MaterialKey k = base;
        k.spv_version = 15;
        CHECK_NE(k.Hash(), base.Hash());
    }
    {
        // Changing variant fields must propagate through variant.Hash()
        MaterialKey k = base;
        k.variant.texture_source_bits = 0xFFFFFFFF;
        CHECK_NE(k.Hash(), base.Hash());
    }
}

// _reserved must NOT affect the hash.
static void test_reserved_does_not_affect_hash()
{
    MaterialKey a = default_key();
    MaterialKey b = default_key();
    b._reserved = 0xDEADBEEFu;
    CHECK_EQ(a.Hash(), b.Hash());
    // Note: operator== is defaulted, so _reserved DOES affect ==.
    // That is intentional — equality is stricter than hash identity.
}

// sizeof must match the computed layout (see ABI CONTRACT in MaterialKey.h).
static void test_size_is_stable()
{
    // Layout:
    //   0  : variant          (MaterialVariantKey, 44 bytes)
    //  44  : pass             (uint8,  1 byte)
    //  45  : [padding]        (1 byte)
    //  46  : def_id           (uint16, 2 bytes)
    //  48  : schema           (uint32, 4 bytes)
    //  52  : glsl_version     (uint16, 2 bytes)
    //  54  : vk_version       (uint16, 2 bytes)
    //  56  : spv_version      (uint16, 2 bytes)
    //  58  : [padding]        (2 bytes)
    //  60  : _reserved        (uint32, 4 bytes)
    // Total: 64 bytes
    static_assert(sizeof(MaterialKey) == 64,
                  "MaterialKey ABI size changed — update layout comment and "
                  "increment the schema version before committing");
    CHECK_EQ(sizeof(MaterialKey), 64u);
}

// MaterialKey must be usable as an unordered_map key via std::hash.
static void test_usable_as_unordered_map_key()
{
    std::unordered_set<MaterialKey> s;
    MaterialKey a = default_key();
    MaterialKey b = default_key();
    b.pass = PassType::ShadowOpaque;

    s.insert(a);
    s.insert(b);
    s.insert(a);   // duplicate — must not grow the set

    CHECK_EQ(s.size(), 2u);
    CHECK_TRUE(s.count(a) == 1);
    CHECK_TRUE(s.count(b) == 1);
}

// operator<=> must induce a strict weak order.
static void test_ordering_is_consistent()
{
    MaterialKey lo = default_key();
    MaterialKey hi = default_key();
    hi.pass = PassType::ShadowOpaque;   // ShadowOpaque > ForwardOpaque numerically

    CHECK_TRUE((lo <=> hi) == std::strong_ordering::less);
    CHECK_TRUE((hi <=> lo) == std::strong_ordering::greater);
    CHECK_TRUE((lo <=> lo) == std::strong_ordering::equal);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_default_construction_stable();
    test_field_changes_affect_hash();
    test_reserved_does_not_affect_hash();
    test_size_is_stable();
    test_usable_as_unordered_map_key();
    test_ordering_is_consistent();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All MaterialKey tests passed.\n");
    return 0;
}
