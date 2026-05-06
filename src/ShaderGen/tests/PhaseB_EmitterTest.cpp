// PhaseB_EmitterTest.cpp
//
// Phase B unit tests for:
//   - MaterialVariantKey::attribute_providers hash behaviour (guard, slot ordering)
//   - EmitAttribInput() / EmitVertexStageInputs() free functions
//
// Standalone executable; no external test framework.
// Success: main() returns 0 and prints "All PhaseB emitter tests passed."
// Failure: any CHECK_* prints a diagnostic to stderr and main() returns 1.

#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/common/AttributeProvider.h>
#include <hgl/shadergen/AttributeProviderRegistry.h>
#include <hgl/common/PositionProvider.h>
#include <hgl/common/DescriptorSetTypeDef.h>

#include <cstdio>
#include <sstream>
#include <string>

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

using namespace hgl::graph;
using namespace hgl::graph::mtl;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool contains(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Two default-constructed keys must have the same hash (all-None → zero hash contribution).
static void test_default_key_hash_stable()
{
    MaterialVariantKey a{};
    MaterialVariantKey b{};
    CHECK_EQ(a.Hash(), b.Hash());
    CHECK_TRUE(a == b);
}

// Setting a single provider must change the hash relative to the default.
static void test_single_provider_changes_hash()
{
    MaterialVariantKey base{};
    MaterialVariantKey modified{};
    modified.SetAttributeProvider(VertexAttrib::Normal, AttributeProviderId::SSBO_Vec3);

    CHECK_NE(base.Hash(), modified.Hash());
    CHECK_TRUE(!(base == modified));
}

// Same provider placed in different semantic slots must yield different hashes.
static void test_different_slots_differ()
{
    MaterialVariantKey key_normal{};
    key_normal.SetAttributeProvider(VertexAttrib::Normal, AttributeProviderId::SSBO_Vec3);

    MaterialVariantKey key_tangent{};
    key_tangent.SetAttributeProvider(VertexAttrib::Tangent, AttributeProviderId::SSBO_Vec3);

    CHECK_NE(key_normal.Hash(), key_tangent.Hash());
    CHECK_TRUE(!(key_normal == key_tangent));
}

// EmitAttribInput on a fully-None key must produce no output.
static void test_emit_attrib_input_all_none_is_empty()
{
    MaterialVariantKey key{};
    std::ostringstream oss;
    EmitAttribInput(oss, key);
    CHECK_TRUE(oss.str().empty());
}

// EmitAttribInput with Normal=SSBO_Vec3 must emit defines + include + undefs.
static void test_emit_attrib_input_single_active()
{
    MaterialVariantKey key{};
    key.SetAttributeProvider(VertexAttrib::Normal, AttributeProviderId::SSBO_Vec3);

    std::ostringstream oss;
    EmitAttribInput(oss, key);
    const std::string s = oss.str();

    CHECK_TRUE(contains(s, "#define ATTRIB_TAG"));
    CHECK_TRUE(contains(s, "Normal"));
    CHECK_TRUE(contains(s, "#define ATTRIB_SET"));
    CHECK_TRUE(contains(s, "#define ATTRIB_BINDING"));
    CHECK_TRUE(contains(s, "#include \""));
    CHECK_TRUE(contains(s, "#undef ATTRIB_TAG"));

    // Normal is semantic index 0 → binding must be 0.
    CHECK_TRUE(contains(s, "#define ATTRIB_BINDING 0"));

    // Set value must match SET_TYPE_VERTEX_STREAMS.
    const std::string expected_set_line =
        std::string("#define ATTRIB_SET     ") + std::to_string(int(SET_TYPE_VERTEX_STREAMS));
    CHECK_TRUE(contains(s, expected_set_line));
}

// EmitVertexStageInputs with DirectVec3 + all-None attribs:
//   position output must appear; no ATTRIB_TAG should appear.
static void test_emit_vertex_stage_inputs_direct_vec3_no_attribs()
{
    MaterialVariantKey key{};

    PositionProvider pos{};
    pos.id        = PositionProviderId::DirectVec3;
    pos.vab_count = 0;

    std::ostringstream oss;
    EmitVertexStageInputs(oss, key, pos, 0);
    const std::string s = oss.str();

    CHECK_TRUE(contains(s, "inPosition"));
    CHECK_TRUE(!contains(s, "ATTRIB_TAG"));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_default_key_hash_stable();
    test_single_provider_changes_hash();
    test_different_slots_differ();
    test_emit_attrib_input_all_none_is_empty();
    test_emit_attrib_input_single_active();
    test_emit_vertex_stage_inputs_direct_vec3_no_attribs();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All PhaseB emitter tests passed.\n");
    return 0;
}
