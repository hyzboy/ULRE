#include <hgl/common/PositionProvider.h>
#include <hgl/shadergen/PositionProviderRegistry.h>

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

using namespace hgl::graph;

static void test_builtin_integer_providers_exist()
{
    const PositionProvider *ivec2 = FindBuiltinProvider(PositionProviderId::VAB_IVec2);
    const PositionProvider *uvec2 = FindBuiltinProvider(PositionProviderId::VAB_UVec2);

    CHECK_TRUE(ivec2 != nullptr);
    CHECK_TRUE(uvec2 != nullptr);

    if (ivec2)
    {
        CHECK_TRUE(ivec2->vab_count == 1);
        CHECK_TRUE(!ivec2->glsl_path.empty());
    }

    if (uvec2)
    {
        CHECK_TRUE(uvec2->vab_count == 1);
        CHECK_TRUE(!uvec2->glsl_path.empty());
    }
}

static void test_builtin_provider_ids_stable_ordering()
{
    CHECK_TRUE(static_cast<unsigned>(PositionProviderId::DirectVec3) == 0u);
    CHECK_TRUE(static_cast<unsigned>(PositionProviderId::VAB_Vec2) == 1u);
    CHECK_TRUE(static_cast<unsigned>(PositionProviderId::VAB_IVec2) == 6u);
    CHECK_TRUE(static_cast<unsigned>(PositionProviderId::VAB_UVec2) == 7u);
}

int main()
{
    test_builtin_integer_providers_exist();
    test_builtin_provider_ids_stable_ordering();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All PositionProviderRegistry tests passed.\n");
    return 0;
}
