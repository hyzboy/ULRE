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

int main()
{
    test_builtin_registry_not_empty_and_iterable();
    test_query_routekey_purecolor3d_hits_registry();
    test_query_unlittexture3d_simple_hits_registry();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All VariantRegistry tests passed.\n");
    return 0;
}
