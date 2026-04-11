#include <cassert>
#include <iostream>

#include <hgl/common/VertexAttribDef.h>
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

static void test_float_accepts_unorm_half_and_float()
{
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R8UN));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R16UN));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R16F));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R32F));
}

static void test_float_rejects_integer_storage()
{
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R8U));
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R32U));
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_FLOAT, PF_R32I));
}

static void test_uint_rejects_unorm_storage()
{
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_UINT, PF_R8UN));
    ASSERT_FALSE(IsStorageFormatCompatibleWithShaderType(VAT_UINT, PF_R16F));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_UINT, PF_R8U));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_UINT, PF_R16U));
    ASSERT_TRUE(IsStorageFormatCompatibleWithShaderType(VAT_UINT, PF_R32U));
}

static void test_luminance_style_spec_is_valid()
{
    VertexAttributeSpec spec{};
    spec.attrib = VAN::Luminance;
    spec.shader_type = VAT_FLOAT;
    spec.storage_format = PF_R8UN;

    ASSERT_TRUE(ValidateVertexAttributeSpec(spec));
}

static void test_invalid_luminance_style_spec_is_rejected()
{
    VertexAttributeSpec spec{};
    spec.attrib = VAN::Luminance;
    spec.shader_type = VAT_FLOAT;
    spec.storage_format = PF_R8U;

    ASSERT_FALSE(ValidateVertexAttributeSpec(spec));
}

int main()
{
    TEST(float_accepts_unorm_half_and_float);
    TEST(float_rejects_integer_storage);
    TEST(uint_rejects_unorm_storage);
    TEST(luminance_style_spec_is_valid);
    TEST(invalid_luminance_style_spec_is_rejected);
    return 0;
}