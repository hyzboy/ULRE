// VKBufferTest – CPU-only unit tests for SSBO buffer utilities.
//
// Tests pure constexpr/inline functions only; no Vulkan device or window required.
// Links CMCore for include paths; no Vulkan runtime dependency.

#include <hgl/common/SSBOOffsetHelper.h>
#include <hgl/common/AttributeProvider.h>

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
// Tests: ComputeVABUsageFlags
// ---------------------------------------------------------------------------
static void test_compute_vab_usage_flags()
{
    // prefer_storage=false: only VERTEX_BUFFER_BIT set
    const uint32_t vbo_only = ComputeVABUsageFlags(false);
    CHECK_TRUE( (vbo_only & kVkBufferUsageVertexBufferBit)  != 0u );
    CHECK_TRUE( (vbo_only & kVkBufferUsageStorageBufferBit) == 0u );

    // prefer_storage=true: both bits set
    const uint32_t with_ssbo = ComputeVABUsageFlags(true);
    CHECK_TRUE( (with_ssbo & kVkBufferUsageVertexBufferBit)  != 0u );
    CHECK_TRUE( (with_ssbo & kVkBufferUsageStorageBufferBit) != 0u );

    // The two return values must differ exactly by STORAGE_BUFFER_BIT
    CHECK_EQ(with_ssbo ^ vbo_only, kVkBufferUsageStorageBufferBit);
}

// ---------------------------------------------------------------------------
// Tests: AlignStorageBufferOffset
// ---------------------------------------------------------------------------
static void test_align_storage_buffer_offset()
{
    // alignment == 0: no constraint, offset returned unchanged
    CHECK_EQ(AlignStorageBufferOffset(7,   0),   7u);
    CHECK_EQ(AlignStorageBufferOffset(0,   0),   0u);
    CHECK_EQ(AlignStorageBufferOffset(255, 0), 255u);

    // offset already aligned: returned unchanged
    CHECK_EQ(AlignStorageBufferOffset(0,   256),   0u);
    CHECK_EQ(AlignStorageBufferOffset(256, 256), 256u);
    CHECK_EQ(AlignStorageBufferOffset(512, 256), 512u);

    // offset not aligned: rounded up
    CHECK_EQ(AlignStorageBufferOffset(1,   256), 256u);
    CHECK_EQ(AlignStorageBufferOffset(255, 256), 256u);
    CHECK_EQ(AlignStorageBufferOffset(257, 256), 512u);

    // Power-of-two alignment == 1: every offset is already aligned
    CHECK_EQ(AlignStorageBufferOffset(7,   1),   7u);

    // Typical GPU alignments
    CHECK_EQ(AlignStorageBufferOffset(12,  64),  64u);
    CHECK_EQ(AlignStorageBufferOffset(64,  64),  64u);
    CHECK_EQ(AlignStorageBufferOffset(65,  64), 128u);
}

// ---------------------------------------------------------------------------
// Tests: GetAttribProviderStride (cross-validate constexpr and spec)
// ---------------------------------------------------------------------------
static_assert(GetAttribProviderStride(AttributeProviderId::SSBO_Vec3) == 12,
              "SSBO_Vec3 must have 12-byte scalar stride");

static void test_stride_ssbo_vec3()
{
    // Runtime double-check — the static_assert above catches compile-time regressions.
    CHECK_EQ(GetAttribProviderStride(AttributeProviderId::SSBO_Vec3), 12u);
    CHECK_EQ(GetAttribProviderStride(AttributeProviderId::SSBO_Vec2),  8u);
    CHECK_EQ(GetAttribProviderStride(AttributeProviderId::SSBO_Vec4), 16u);
    CHECK_EQ(GetAttribProviderStride(AttributeProviderId::None),       0u);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    test_compute_vab_usage_flags();
    test_align_storage_buffer_offset();
    test_stride_ssbo_vec3();

    if (g_failures == 0)
    {
        std::fprintf(stdout, "All VKBuffer tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "%d VKBuffer test(s) FAILED.\n", g_failures);
    return 1;
}
