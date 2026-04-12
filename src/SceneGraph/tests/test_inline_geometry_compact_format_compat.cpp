#include <iostream>
#include <cstdlib>

#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/VertexFormatMap.h>
#include <hgl/mtl/VertexAttributeSpec.h>
#include <hgl/vk/VKFormat.h>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

#define TEST(name) \
    std::cout << "Testing " << #name << "... "; \
    test_##name(); \
    std::cout << "PASSED" << std::endl;

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "FAILED: " << #expr << " at line " << __LINE__ << std::endl; \
            std::exit(1); \
        } \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

static void test_vec2_float_accepts_compact_normal_storage()
{
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_VEC4, PF_RGBA8SN));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_VEC4, PF_RGBA16F));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_VEC4, PF_A2RGB10SN));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_VEC4, PF_A2BGR10SN));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_VEC4, PF_RGB32F));
}

static void test_vec3_float_rejects_vec4_only_packed_storage()
{
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_VEC3, PF_RGBA8SN));
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_VEC3, PF_RGBA16F));
}

static void test_compact_maps_have_expected_storage_formats()
{
    ASSERT_TRUE(vfmt::kLitSurfaceNT_SN8x4_SN8x4_UV_HF16x2.at(VAN::Normal) == PF_RGBA8SN);
    ASSERT_TRUE(vfmt::kLitSurfaceNT_SN8x4_SN8x4_UV_HF16x2.at(VAN::Tangent) == PF_RGBA8SN);
    ASSERT_TRUE(vfmt::kLitSurfaceNT_SN8x4_SN8x4_UV_HF16x2.at(VAN::TexCoord) == PF_RG16F);

    ASSERT_TRUE(vfmt::kLitSurfaceNT_HF16x4_HF16x4_UV_HF16x2.at(VAN::Normal) == PF_RGBA16F);
    ASSERT_TRUE(vfmt::kLitSurfaceNT_HF16x4_HF16x4_UV_HF16x2.at(VAN::Tangent) == PF_RGBA16F);
    ASSERT_TRUE(vfmt::kLitSurfaceNT_HF16x4_HF16x4_UV_HF16x2.at(VAN::TexCoord) == PF_RG16F);

    ASSERT_TRUE(vfmt::kLitSurfaceNT_A2BGR10SN_A2BGR10SN_UV_HF16x2.at(VAN::Normal) == PF_A2BGR10SN);
    ASSERT_TRUE(vfmt::kLitSurfaceNT_A2BGR10SN_A2BGR10SN_UV_HF16x2.at(VAN::Tangent) == PF_A2BGR10SN);
    ASSERT_TRUE(vfmt::kLitSurfaceNT_A2BGR10SN_A2BGR10SN_UV_HF16x2.at(VAN::TexCoord) == PF_RG16F);
}

int main()
{
    TEST(vec2_float_accepts_compact_normal_storage);
    TEST(vec3_float_rejects_vec4_only_packed_storage);
    TEST(compact_maps_have_expected_storage_formats);
    return 0;
}
