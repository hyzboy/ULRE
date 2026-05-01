// AttributeProviderRegistry unit tests – standalone executable, no external test framework.
//
// Failure: any CHECK_* macro prints a message and increments g_failures.
// Success: main() returns 0.

#include <hgl/shadergen/AttributeProviderRegistry.h>

#include <cstdio>

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

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

// Every documented built-in ID must be present and return a non-null record.
static void test_all_builtin_ids_are_found()
{
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::None)                 != nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::SSBO_Vec2)            != nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::SSBO_Vec3)            != nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::SSBO_Vec4)            != nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::SSBO_PackedRGBA8)     != nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::SSBO_PackedNormal_Oct)!= nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::SSBO_PackedUV_2x16)   != nullptr);
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::Constant)             != nullptr);
}

// The returned record must carry the expected ID (no table corruption).
static void test_returned_record_id_matches()
{
    const AttributeProvider *p = FindBuiltinAttribProvider(AttributeProviderId::SSBO_Vec3);
    CHECK_TRUE(p != nullptr);
    if (p) CHECK_EQ(p->id, AttributeProviderId::SSBO_Vec3);
}

// SSBO providers must require an SSBO binding; Constant must not.
static void test_ssbo_flags()
{
    const AttributeProvider *vec3 = FindBuiltinAttribProvider(AttributeProviderId::SSBO_Vec3);
    CHECK_TRUE(vec3 != nullptr);
    if (vec3) CHECK_TRUE(vec3->needs_ssbo);

    const AttributeProvider *constant = FindBuiltinAttribProvider(AttributeProviderId::Constant);
    CHECK_TRUE(constant != nullptr);
    if (constant) CHECK_TRUE(!constant->needs_ssbo);
}

// Packed providers must have byte_stride == 4; Vec4 must have byte_stride == 16.
static void test_byte_stride_sanity()
{
    const AttributeProvider *rgba8 = FindBuiltinAttribProvider(AttributeProviderId::SSBO_PackedRGBA8);
    CHECK_TRUE(rgba8 != nullptr);
    if (rgba8) CHECK_EQ(rgba8->byte_stride, static_cast<hgl::uint8>(4));

    const AttributeProvider *uv16 = FindBuiltinAttribProvider(AttributeProviderId::SSBO_PackedUV_2x16);
    CHECK_TRUE(uv16 != nullptr);
    if (uv16) CHECK_EQ(uv16->byte_stride, static_cast<hgl::uint8>(4));

    const AttributeProvider *vec4 = FindBuiltinAttribProvider(AttributeProviderId::SSBO_Vec4);
    CHECK_TRUE(vec4 != nullptr);
    if (vec4) CHECK_EQ(vec4->byte_stride, static_cast<hgl::uint8>(16));
}

// An unknown user-defined ID must return nullptr.
static void test_unknown_id_returns_nullptr()
{
    CHECK_TRUE(FindBuiltinAttribProvider(AttributeProviderId::UserCustom_Begin) == nullptr);
    // Arbitrary unknown value just below UserCustom_Begin
    CHECK_TRUE(FindBuiltinAttribProvider(static_cast<AttributeProviderId>(999)) == nullptr);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_all_builtin_ids_are_found();
    test_returned_record_id_matches();
    test_ssbo_flags();
    test_byte_stride_sanity();
    test_unknown_id_returns_nullptr();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All AttributeProviderRegistry tests passed.\n");
    return 0;
}
