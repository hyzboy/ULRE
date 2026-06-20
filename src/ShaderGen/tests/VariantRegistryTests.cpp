// VariantRegistry unit tests - standalone executable, no external test framework.
//
// Failure: any CHECK_* macro prints a message and increments g_failures.
// Success: main() returns 0.

#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/mtl/MaterialLibrary.h>

#include <cstdio>

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b) CHECK_TRUE((a) == (b))

using namespace hgl::graph::mtl;
using namespace hgl::graph;

static void test_builtin_registry_not_empty_and_iterable()
{
    const VariantRegistry &registry = GetBuiltinVariantRegistry();

    CHECK_TRUE(registry.Size() > 0);

    size_t variant_count = 0;
    registry.ForEach([&](const MaterialVariantKey &, const MaterialVariantDesc &) {
        ++variant_count;
    });

    CHECK_EQ(variant_count, registry.Size());

    size_t row_count = 0;
    registry.ForEachBuiltinRow([&](const MaterialVariantRow &) {
        ++row_count;
    });

    CHECK_TRUE(row_count > 0);
}

static void test_query_routekey_purecolor3d_hits_registry()
{
    const VariantRegistry &registry = GetBuiltinVariantRegistry();

    MaterialVariantKey key = RouteKey(MaterialPreset::PureColor3D);

    RegistryLookupOptions options;
    options.preferred_factory_type = MaterialPreset::PureColor3D;

    MaterialVariantKey resolved{};
    const MaterialVariantDesc *desc = registry.QueryVariantWithCanonicalFallback(key, &resolved, options);

    if (!desc)
    {
        std::fprintf(stderr,
                     "[VariantRegistryTests] PureColor3D miss: surface=%u geom=%u pos=%u blend=%u pass=%u tex_bits=0x%08X\n",
                     static_cast<unsigned>(key.surface_type),
                     static_cast<unsigned>(key.geometry_mode),
                     static_cast<unsigned>(key.position_provider),
                     static_cast<unsigned>(key.blend_mode),
                     static_cast<unsigned>(key.pass_hint),
                     key.texture_source_bits);
    }

    CHECK_TRUE(desc != nullptr);
    if (desc)
        CHECK_TRUE(!desc->variant_name.empty());
}

static void test_query_unlittexture3d_simple_hits_registry()
{
    const VariantRegistry &registry = GetBuiltinVariantRegistry();

    MaterialVariantKey key = RouteKey(MaterialPreset::UnlitTexture3D);
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    key.blend_mode = RenderAlphaMode::Transparent;
    key.pass_hint = PassType::ForwardTransparent;

    RegistryLookupOptions options;
    options.preferred_factory_type = MaterialPreset::UnlitTexture3D;

    const MaterialVariantDesc *desc = registry.QueryVariantWithCanonicalFallback(key, nullptr, options);

    if (!desc)
    {
        std::fprintf(stderr,
                     "[VariantRegistryTests] UnlitTexture3D miss: surface=%u geom=%u pos=%u blend=%u pass=%u tex_bits=0x%08X\n",
                     static_cast<unsigned>(key.surface_type),
                     static_cast<unsigned>(key.geometry_mode),
                     static_cast<unsigned>(key.position_provider),
                     static_cast<unsigned>(key.blend_mode),
                     static_cast<unsigned>(key.pass_hint),
                     key.texture_source_bits);
    }

    CHECK_TRUE(desc != nullptr);
    if (desc)
        CHECK_TRUE(!desc->variant_name.empty());
}

static void test_preferred_factory_type_picks_expected_candidate()
{
    VariantRegistry registry;

    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.position_provider = PositionProviderId::DirectVec3;
    key.blend_mode = RenderAlphaMode::Opaque;
    key.pass_hint = PassType::ForwardOpaque;

    MaterialVariantDesc desc_a;
    desc_a.variant_name = "candidate_A";
    desc_a.factory_type = MaterialPreset::PureColor3D;

    MaterialVariantDesc desc_b;
    desc_b.variant_name = "candidate_B";
    desc_b.factory_type = MaterialPreset::VertexColor3D;

    registry.RegisterVariant(key, desc_a);
    registry.RegisterVariant(key, desc_b);

    const MaterialVariantDesc *default_pick = registry.QueryVariant(key);
    CHECK_TRUE(default_pick != nullptr);

    RegistryLookupOptions opt{};
    opt.preferred_factory_type = MaterialPreset::VertexColor3D;

    const MaterialVariantDesc *preferred_pick = registry.QueryVariant(key, opt);
    CHECK_TRUE(preferred_pick != nullptr);
    if (preferred_pick)
        CHECK_TRUE(preferred_pick->variant_name == "candidate_B");
}

static void test_effective_feature_mask_canonicalization_behavior()
{
    VariantRegistry registry;

    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.position_provider = PositionProviderId::DirectVec3;
    key.blend_mode = RenderAlphaMode::Opaque;
    key.pass_hint = PassType::ForwardOpaque;
    key.effective_feature_mask = 0;

    MaterialVariantDesc desc;
    desc.variant_name = "mask_canonical_base";
    desc.factory_type = MaterialPreset::PureColor3D;
    registry.RegisterVariant(key, desc);

    MaterialVariantKey query = key;
    query.effective_feature_mask = 0x1234u;

    MaterialVariantKey resolved{};
    const MaterialVariantDesc *canonical_hit = registry.QueryVariantWithCanonicalFallback(query, &resolved);
    CHECK_TRUE(canonical_hit != nullptr);
    CHECK_EQ(resolved.effective_feature_mask, 0u);

    RegistryLookupOptions strict_opt{};
    strict_opt.match_effective_feature_mask = true;
    const MaterialVariantDesc *strict_miss = registry.QueryVariantWithCanonicalFallback(query, nullptr, strict_opt);
    CHECK_TRUE(strict_miss == nullptr);
}

int main()
{
    test_builtin_registry_not_empty_and_iterable();
    test_query_routekey_purecolor3d_hits_registry();
    test_query_unlittexture3d_simple_hits_registry();
    test_preferred_factory_type_picks_expected_candidate();
    test_effective_feature_mask_canonicalization_behavior();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All VariantRegistry tests passed.\n");
    return 0;
}
