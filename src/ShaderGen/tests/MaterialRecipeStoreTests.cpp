// MaterialRecipeStore unit tests - standalone executable, no external test framework.
//
// Failure: any CHECK_* macro prints a message and increments g_failures.
// Success: main() returns 0.

#include <hgl/mtl/MaterialRecipeStore.h>
#include <hgl/mtl/MaterialRecipeID.h>

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

using namespace hgl::graph::mtl;

static MaterialRecipe MakeSimpleRecipe(const char *id_str)
{
    MaterialRecipe r;
    r.id = id_str;
    r.preset = MaterialPreset::Standard;
    r.dim = MaterialRecipe::Dim::D3;
    return r;
}

// ---------------------------------------------------------------------------
// Test: RegisterRecipe returns a valid ID (> 0)
// ---------------------------------------------------------------------------
static void TestRegisterReturnsValidID()
{
    MaterialRecipeStore store;
    MaterialRecipe r = MakeSimpleRecipe("test_recipe_a");
    MaterialRecipeID id = store.RegisterRecipe(r);
    CHECK_NE(id, kInvalidMaterialRecipeID);
    std::fprintf(stdout, "TestRegisterReturnsValidID: id=%u\n", id);
}

// ---------------------------------------------------------------------------
// Test: Two identical recipes deduplicate to the same ID
// ---------------------------------------------------------------------------
static void TestDeduplication()
{
    MaterialRecipeStore store;
    MaterialRecipe r = MakeSimpleRecipe("dup_recipe");
    MaterialRecipeID id1 = store.RegisterRecipe(r);
    MaterialRecipeID id2 = store.RegisterRecipe(r);
    CHECK_EQ(id1, id2);
    CHECK_EQ(store.Size(), static_cast<size_t>(1));
    std::fprintf(stdout, "TestDeduplication: id1=%u id2=%u size=%zu\n", id1, id2, store.Size());
}

// ---------------------------------------------------------------------------
// Test: Two distinct recipes get different IDs
// ---------------------------------------------------------------------------
static void TestDistinctRecipesDifferentIDs()
{
    MaterialRecipeStore store;
    MaterialRecipe a = MakeSimpleRecipe("recipe_a");
    MaterialRecipe b = MakeSimpleRecipe("recipe_b");
    MaterialRecipeID id_a = store.RegisterRecipe(a);
    MaterialRecipeID id_b = store.RegisterRecipe(b);
    CHECK_NE(id_a, id_b);
    CHECK_EQ(store.Size(), static_cast<size_t>(2));
    std::fprintf(stdout, "TestDistinctRecipesDifferentIDs: id_a=%u id_b=%u\n", id_a, id_b);
}

// ---------------------------------------------------------------------------
// Test: GetRecipe returns pointer to correct recipe
// ---------------------------------------------------------------------------
static void TestGetRecipe()
{
    MaterialRecipeStore store;
    MaterialRecipe r = MakeSimpleRecipe("getrecipe_test");
    MaterialRecipeID id = store.RegisterRecipe(r);
    const MaterialRecipe *ptr = store.GetRecipe(id);
    CHECK_TRUE(ptr != nullptr);
    CHECK_TRUE(ptr->id == r.id);
    std::fprintf(stdout, "TestGetRecipe: id=%u ptr->id=%s\n", id, ptr ? ptr->id.c_str() : "(null)");
}

// ---------------------------------------------------------------------------
// Test: GetRecipe with invalid ID returns nullptr
// ---------------------------------------------------------------------------
static void TestGetRecipeInvalidID()
{
    MaterialRecipeStore store;
    CHECK_TRUE(store.GetRecipe(kInvalidMaterialRecipeID) == nullptr);
    CHECK_TRUE(store.GetRecipe(999u) == nullptr);
    std::fprintf(stdout, "TestGetRecipeInvalidID: OK\n");
}

// ---------------------------------------------------------------------------
// Test: FindByContentHash round-trips
// ---------------------------------------------------------------------------
static void TestFindByContentHash()
{
    MaterialRecipeStore store;
    MaterialRecipe r = MakeSimpleRecipe("hash_test");
    MaterialRecipeID id = store.RegisterRecipe(r);
    uint64_t hash = MaterialRecipeStore::ComputeContentHash(r);
    MaterialRecipeID found = store.FindByContentHash(hash);
    CHECK_EQ(found, id);
    std::fprintf(stdout, "TestFindByContentHash: id=%u found=%u hash=%llu\n", id, found,
                 static_cast<unsigned long long>(hash));
}

// ---------------------------------------------------------------------------
// Test: UpdateRecipe changes content and rehashes
// ---------------------------------------------------------------------------
static void TestUpdateRecipe()
{
    MaterialRecipeStore store;
    MaterialRecipe orig = MakeSimpleRecipe("update_orig");
    MaterialRecipeID id = store.RegisterRecipe(orig);

    MaterialRecipe updated = MakeSimpleRecipe("update_new");
    bool ok = store.UpdateRecipe(id, updated);
    CHECK_TRUE(ok);

    const MaterialRecipe *ptr = store.GetRecipe(id);
    CHECK_TRUE(ptr != nullptr);
    CHECK_TRUE(ptr->id == updated.id);

    // Old hash no longer resolvable
    uint64_t old_hash = MaterialRecipeStore::ComputeContentHash(orig);
    CHECK_EQ(store.FindByContentHash(old_hash), kInvalidMaterialRecipeID);

    // New hash resolves
    uint64_t new_hash = MaterialRecipeStore::ComputeContentHash(updated);
    CHECK_EQ(store.FindByContentHash(new_hash), id);

    std::fprintf(stdout, "TestUpdateRecipe: id=%u new_id=%s OK\n", id, ptr->id.c_str());
}

// ---------------------------------------------------------------------------
// Test: UpdateRecipe with invalid ID returns false
// ---------------------------------------------------------------------------
static void TestUpdateRecipeInvalidID()
{
    MaterialRecipeStore store;
    MaterialRecipe r = MakeSimpleRecipe("dummy");
    CHECK_TRUE(!store.UpdateRecipe(kInvalidMaterialRecipeID, r));
    CHECK_TRUE(!store.UpdateRecipe(999u, r));
    std::fprintf(stdout, "TestUpdateRecipeInvalidID: OK\n");
}

// ---------------------------------------------------------------------------
// Test: Phase B explicit-axis fields participate in content hash/dedup identity
// ---------------------------------------------------------------------------
static void TestPhaseBExplicitAxesAffectHash()
{
    MaterialRecipeStore store;

    MaterialRecipe base = MakeSimpleRecipe("phaseb_axes");
    MaterialRecipe axis = base;
    axis.vertex_policy = VertexTransformPolicy::Mesh3D;
    axis.has_explicit_schema = true;
    axis.schema = ShaderDataSchema::StandardParams;

    const uint64_t base_hash = MaterialRecipeStore::ComputeContentHash(base);
    const uint64_t axis_hash = MaterialRecipeStore::ComputeContentHash(axis);

    CHECK_NE(base_hash, axis_hash);

    const MaterialRecipeID id_base = store.RegisterRecipe(base);
    const MaterialRecipeID id_axis = store.RegisterRecipe(axis);
    CHECK_NE(id_base, id_axis);

    std::fprintf(stdout,
        "TestPhaseBExplicitAxesAffectHash: base_hash=%llu axis_hash=%llu id_base=%u id_axis=%u\n",
        static_cast<unsigned long long>(base_hash),
        static_cast<unsigned long long>(axis_hash),
        id_base,
        id_axis);
}

// ---------------------------------------------------------------------------
int main()
{
    TestRegisterReturnsValidID();
    TestDeduplication();
    TestDistinctRecipesDifferentIDs();
    TestGetRecipe();
    TestGetRecipeInvalidID();
    TestFindByContentHash();
    TestUpdateRecipe();
    TestUpdateRecipeInvalidID();
    TestPhaseBExplicitAxesAffectHash();

    if (g_failures == 0)
        std::fprintf(stdout, "All tests PASSED.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
