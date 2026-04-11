#include <iostream>
#include <string>

#include <hgl/common/VertexInputDef.h>
#include <hgl/common/InterpolationDef.h>
#include <hgl/graph/module/VertexBindingCompatibility.h>
#include <hgl/vk/VKVertexInput.h>

using namespace hgl::graph;

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

static VertexInput *CreateTestVertexInput()
{
    VIAArray via_array;
    ASSERT_TRUE(via_array.Init(2));

    via_array.items[0] = {};
    via_array.items[0].attrib = VAN::Position;
    via_array.items[0].location = 0;
    via_array.items[0].basetype = uint8_t(VABaseType::Float);
    via_array.items[0].vec_size = 3;
    via_array.items[0].storage_format = PF_RGB32F;
    via_array.items[0].interpolation = Interpolation::Smooth;

    via_array.items[1] = {};
    via_array.items[1].attrib = VAN::Luminance;
    via_array.items[1].location = 1;
    via_array.items[1].basetype = uint8_t(VABaseType::Float);
    via_array.items[1].vec_size = 1;
    via_array.items[1].storage_format = PF_R32F;
    via_array.items[1].interpolation = Interpolation::Smooth;

    return GetVertexInput(via_array);
}

static void test_accepts_compatible_storage_format()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    std::string reason;
    ASSERT_TRUE(IsMaterialVertexAttribStorageCompatible(vi, VAN::Luminance, PF_R8UN, &reason));

    ReleaseVertexInput(vi);
}

static void test_rejects_incompatible_storage_format()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    std::string reason;
    ASSERT_FALSE(IsMaterialVertexAttribStorageCompatible(vi, VAN::Luminance, PF_R8U, &reason));
    ASSERT_TRUE(reason == "shader_storage_incompatible");

    ReleaseVertexInput(vi);
}

static void test_rejects_missing_material_attrib()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    std::string reason;
    ASSERT_FALSE(IsMaterialVertexAttribStorageCompatible(vi, VAN::TexCoord, PF_RG16F, &reason));
    ASSERT_TRUE(reason == "material_vertex_input_missing_attrib");

    ReleaseVertexInput(vi);
}

static void test_rejects_missing_material_on_storage_check()
{
    std::string reason;
    ASSERT_FALSE(IsMaterialStorageCompatible(nullptr, VAN::Position, PF_RGB32F, &reason));
    ASSERT_TRUE(reason == "material_missing");
}

static void test_build_geometry_driven_vil_config_requires_material()
{
    VILConfig cfg;
    bool has_any = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfig(nullptr, nullptr, nullptr, cfg, has_any, &reason, nullptr));
    ASSERT_TRUE(reason == "material_missing");
    ASSERT_TRUE(cfg.empty());
}

static void test_rejects_null_vertex_input_as_missing_attrib()
{
    std::string reason;
    ASSERT_FALSE(IsMaterialVertexAttribStorageCompatible(nullptr, VAN::Position, PF_RGB32F, &reason));
    ASSERT_TRUE(reason == "material_vertex_input_missing_attrib");
}

static void test_null_reason_pointer_is_safe()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    ASSERT_TRUE(IsMaterialVertexAttribStorageCompatible(vi, VAN::Luminance, PF_R8UN, nullptr));
    ASSERT_FALSE(IsMaterialVertexAttribStorageCompatible(vi, VAN::Luminance, PF_R8U, nullptr));

    ReleaseVertexInput(vi);
}

static void test_build_geometry_driven_vil_config_requires_geometry()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    VILConfig cfg;
    bool has_any = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfigFromVertexInput(vi, nullptr, nullptr, cfg, has_any, &reason, nullptr));
    ASSERT_TRUE(reason == "geometry_missing");
    ASSERT_TRUE(has_any == false);
    ASSERT_TRUE(cfg.empty());

    ReleaseVertexInput(vi);
}

static void test_build_geometry_driven_vil_config_geometry_check_precedes_vi_check()
{
    VILConfig cfg;
    bool has_any = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfigFromVertexInput(nullptr, nullptr, nullptr, cfg, has_any, &reason, nullptr));
    ASSERT_TRUE(reason == "geometry_missing");
    ASSERT_TRUE(has_any == false);
    ASSERT_TRUE(cfg.empty());
}

int main()
{
    TEST(accepts_compatible_storage_format);
    TEST(rejects_incompatible_storage_format);
    TEST(rejects_missing_material_attrib);
    TEST(rejects_missing_material_on_storage_check);
    TEST(build_geometry_driven_vil_config_requires_material);
    TEST(rejects_null_vertex_input_as_missing_attrib);
    TEST(null_reason_pointer_is_safe);
    TEST(build_geometry_driven_vil_config_requires_geometry);
    TEST(build_geometry_driven_vil_config_geometry_check_precedes_vi_check);
    return 0;
}
