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
    bool has_layout_mismatch = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfig(nullptr, nullptr, nullptr, cfg, has_any, &reason, &has_layout_mismatch));
    ASSERT_TRUE(reason == "material_missing");
    ASSERT_TRUE(has_any == false);
    ASSERT_TRUE(has_layout_mismatch == false);
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
    bool has_layout_mismatch = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfigFromVertexInput(vi, nullptr, nullptr, cfg, has_any, &reason, &has_layout_mismatch));
    ASSERT_TRUE(reason == "geometry_missing");
    ASSERT_TRUE(has_any == false);
    ASSERT_TRUE(has_layout_mismatch == false);
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

static void test_resolver_entrypoint_success_without_mismatch()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    const VIL *requested_vil = vi->GetDefaultVIL();
    ASSERT_TRUE(requested_vil != nullptr);

    auto resolver = [&](const VertexAttrib attrib,
                        const VkVertexInputRate requested_input_rate,
                        bool &has_storage,
                        VkFormat &storage_format,
                        uint32_t &storage_stride,
                        VkVertexInputRate &storage_input_rate) -> bool
    {
        has_storage = true;
        storage_input_rate = requested_input_rate;

        const VertexInputFormat *cfg = requested_vil->GetConfig(attrib);
        ASSERT_TRUE(cfg != nullptr);

        storage_format = cfg->format;
        storage_stride = cfg->stride;
        return true;
    };

    VILConfig out_cfg;
    bool has_any = false;
    bool has_layout_mismatch = false;
    std::string reason;

    ASSERT_TRUE(BuildGeometryDrivenVILConfigFromVertexInputWithResolver(vi,
                                                                        requested_vil,
                                                                        resolver,
                                                                        out_cfg,
                                                                        has_any,
                                                                        &reason,
                                                                        &has_layout_mismatch));
    ASSERT_TRUE(has_any == true);
    ASSERT_TRUE(has_layout_mismatch == false);
    ASSERT_TRUE(out_cfg.GetCount() == int(requested_vil->GetVertexAttribCount()));

    ReleaseVertexInput(vi);
}

static void test_resolver_entrypoint_detects_layout_mismatch()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    const VIL *requested_vil = vi->GetDefaultVIL();
    ASSERT_TRUE(requested_vil != nullptr);

    auto resolver = [&](const VertexAttrib attrib,
                        const VkVertexInputRate requested_input_rate,
                        bool &has_storage,
                        VkFormat &storage_format,
                        uint32_t &storage_stride,
                        VkVertexInputRate &storage_input_rate) -> bool
    {
        has_storage = true;
        storage_input_rate = requested_input_rate;

        const VertexInputFormat *cfg = requested_vil->GetConfig(attrib);
        ASSERT_TRUE(cfg != nullptr);

        storage_format = cfg->format;
        storage_stride = cfg->stride;

        if(attrib == VAN::Luminance)
        {
            storage_format = PF_R8UN;
            storage_stride = GetStrideByFormat(storage_format);
        }

        return true;
    };

    VILConfig out_cfg;
    bool has_any = false;
    bool has_layout_mismatch = false;
    std::string reason;

    ASSERT_TRUE(BuildGeometryDrivenVILConfigFromVertexInputWithResolver(vi,
                                                                        requested_vil,
                                                                        resolver,
                                                                        out_cfg,
                                                                        has_any,
                                                                        &reason,
                                                                        &has_layout_mismatch));
    ASSERT_TRUE(has_any == true);
    ASSERT_TRUE(has_layout_mismatch == true);
    ASSERT_TRUE(out_cfg.GetCount() == int(requested_vil->GetVertexAttribCount()));

    ReleaseVertexInput(vi);
}

static void test_resolver_entrypoint_reports_lookup_failure()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    const VIL *requested_vil = vi->GetDefaultVIL();
    ASSERT_TRUE(requested_vil != nullptr);

    auto resolver = [&](const VertexAttrib,
                        const VkVertexInputRate,
                        bool &,
                        VkFormat &,
                        uint32_t &,
                        VkVertexInputRate &) -> bool
    {
        return false;
    };

    VILConfig out_cfg;
    bool has_any = true;
    bool has_layout_mismatch = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfigFromVertexInputWithResolver(vi,
                                                                         requested_vil,
                                                                         resolver,
                                                                         out_cfg,
                                                                         has_any,
                                                                         &reason,
                                                                         &has_layout_mismatch));
    ASSERT_TRUE(reason == "storage_lookup_failed");
    ASSERT_TRUE(has_any == false);
    ASSERT_TRUE(has_layout_mismatch == false);
    ASSERT_TRUE(out_cfg.empty());

    ReleaseVertexInput(vi);
}

static void test_resolver_entrypoint_reports_incompatible_storage()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    const VIL *requested_vil = vi->GetDefaultVIL();
    ASSERT_TRUE(requested_vil != nullptr);

    auto resolver = [&](const VertexAttrib attrib,
                        const VkVertexInputRate requested_input_rate,
                        bool &has_storage,
                        VkFormat &storage_format,
                        uint32_t &storage_stride,
                        VkVertexInputRate &storage_input_rate) -> bool
    {
        has_storage = true;
        storage_input_rate = requested_input_rate;

        const VertexInputFormat *cfg = requested_vil->GetConfig(attrib);
        ASSERT_TRUE(cfg != nullptr);

        storage_format = cfg->format;
        storage_stride = cfg->stride;

        if(attrib == VAN::Luminance)
        {
            storage_format = PF_R8U;
            storage_stride = GetStrideByFormat(storage_format);
        }

        return true;
    };

    VILConfig out_cfg;
    bool has_any = true;
    bool has_layout_mismatch = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfigFromVertexInputWithResolver(vi,
                                                                         requested_vil,
                                                                         resolver,
                                                                         out_cfg,
                                                                         has_any,
                                                                         &reason,
                                                                         &has_layout_mismatch));
    ASSERT_TRUE(reason == "shader_storage_incompatible");

    ReleaseVertexInput(vi);
}

static void test_resolver_entrypoint_reports_missing_required_attrib()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    const VIL *requested_vil = vi->GetDefaultVIL();
    ASSERT_TRUE(requested_vil != nullptr);

    auto resolver = [&](const VertexAttrib attrib,
                        const VkVertexInputRate requested_input_rate,
                        bool &has_storage,
                        VkFormat &storage_format,
                        uint32_t &storage_stride,
                        VkVertexInputRate &storage_input_rate) -> bool
    {
        storage_input_rate = requested_input_rate;

        if(attrib == VAN::Luminance)
        {
            has_storage = false;
            return true;
        }

        has_storage = true;
        const VertexInputFormat *cfg = requested_vil->GetConfig(attrib);
        ASSERT_TRUE(cfg != nullptr);

        storage_format = cfg->format;
        storage_stride = cfg->stride;
        return true;
    };

    VILConfig out_cfg;
    bool has_any = true;
    bool has_layout_mismatch = true;
    std::string reason;

    ASSERT_FALSE(BuildGeometryDrivenVILConfigFromVertexInputWithResolver(vi,
                                                                         requested_vil,
                                                                         resolver,
                                                                         out_cfg,
                                                                         has_any,
                                                                         &reason,
                                                                         &has_layout_mismatch));
    ASSERT_TRUE(reason == "vab_missing_for_required_attrib");

    ReleaseVertexInput(vi);
}

static void test_resolver_entrypoint_no_requested_vil_all_missing_is_ok()
{
    VertexInput *vi = CreateTestVertexInput();
    ASSERT_TRUE(vi != nullptr);

    auto resolver = [&](const VertexAttrib,
                        const VkVertexInputRate,
                        bool &has_storage,
                        VkFormat &,
                        uint32_t &,
                        VkVertexInputRate &) -> bool
    {
        has_storage = false;
        return true;
    };

    VILConfig out_cfg;
    bool has_any = true;
    bool has_layout_mismatch = true;
    std::string reason;

    ASSERT_TRUE(BuildGeometryDrivenVILConfigFromVertexInputWithResolver(vi,
                                                                        nullptr,
                                                                        resolver,
                                                                        out_cfg,
                                                                        has_any,
                                                                        &reason,
                                                                        &has_layout_mismatch));
    ASSERT_TRUE(has_any == false);
    ASSERT_TRUE(has_layout_mismatch == false);
    ASSERT_TRUE(out_cfg.empty());

    ReleaseVertexInput(vi);
}

static void test_runtime_vil_release_clears_active_when_owned_is_active()
{
    const VIL *active_vil = reinterpret_cast<const VIL *>(0x100);
    VIL *owned_runtime_vil = reinterpret_cast<VIL *>(0x100);
    int release_count = 0;

    ReleaseOwnedRuntimeVIL(active_vil, owned_runtime_vil, [&](VIL *v)
    {
        ASSERT_TRUE(v == reinterpret_cast<VIL *>(0x100));
        ++release_count;
    });

    ASSERT_TRUE(release_count == 1);
    ASSERT_TRUE(active_vil == nullptr);
    ASSERT_TRUE(owned_runtime_vil == nullptr);
}

static void test_runtime_vil_release_preserves_active_when_not_owned()
{
    const VIL *active_vil = reinterpret_cast<const VIL *>(0x200);
    VIL *owned_runtime_vil = reinterpret_cast<VIL *>(0x100);
    int release_count = 0;

    ReleaseOwnedRuntimeVIL(active_vil, owned_runtime_vil, [&](VIL *v)
    {
        ASSERT_TRUE(v == reinterpret_cast<VIL *>(0x100));
        ++release_count;
    });

    ASSERT_TRUE(release_count == 1);
    ASSERT_TRUE(active_vil == reinterpret_cast<const VIL *>(0x200));
    ASSERT_TRUE(owned_runtime_vil == nullptr);
}

static void test_runtime_vil_replace_direct_rebind_releases_old_runtime()
{
    const VIL *active_vil = reinterpret_cast<const VIL *>(0x100);
    VIL *owned_runtime_vil = reinterpret_cast<VIL *>(0x100);

    const VIL *requested_vil = reinterpret_cast<const VIL *>(0x300);
    VIL *new_owned_runtime_vil = nullptr;

    int release_count = 0;

    ReplaceRuntimeVILBinding(active_vil,
                             owned_runtime_vil,
                             requested_vil,
                             new_owned_runtime_vil,
                             [&](VIL *v)
                             {
                                 ASSERT_TRUE(v == reinterpret_cast<VIL *>(0x100));
                                 ++release_count;
                             });

    ASSERT_TRUE(release_count == 1);
    ASSERT_TRUE(active_vil == requested_vil);
    ASSERT_TRUE(owned_runtime_vil == nullptr);
}

static void test_runtime_vil_replace_deferred_rebind_releases_old_and_sets_new()
{
    const VIL *active_vil = reinterpret_cast<const VIL *>(0x100);
    VIL *owned_runtime_vil = reinterpret_cast<VIL *>(0x100);

    const VIL *new_active_vil = reinterpret_cast<const VIL *>(0x400);
    VIL *new_owned_runtime_vil = reinterpret_cast<VIL *>(0x400);

    int release_count = 0;

    ReplaceRuntimeVILBinding(active_vil,
                             owned_runtime_vil,
                             new_active_vil,
                             new_owned_runtime_vil,
                             [&](VIL *v)
                             {
                                 ASSERT_TRUE(v == reinterpret_cast<VIL *>(0x100));
                                 ++release_count;
                             });

    ASSERT_TRUE(release_count == 1);
    ASSERT_TRUE(active_vil == new_active_vil);
    ASSERT_TRUE(owned_runtime_vil == new_owned_runtime_vil);
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
    TEST(resolver_entrypoint_success_without_mismatch);
    TEST(resolver_entrypoint_detects_layout_mismatch);
    TEST(resolver_entrypoint_reports_lookup_failure);
    TEST(resolver_entrypoint_reports_incompatible_storage);
    TEST(resolver_entrypoint_reports_missing_required_attrib);
    TEST(resolver_entrypoint_no_requested_vil_all_missing_is_ok);
    TEST(runtime_vil_release_clears_active_when_owned_is_active);
    TEST(runtime_vil_release_preserves_active_when_not_owned);
    TEST(runtime_vil_replace_direct_rebind_releases_old_runtime);
    TEST(runtime_vil_replace_deferred_rebind_releases_old_and_sets_new);
    return 0;
}
