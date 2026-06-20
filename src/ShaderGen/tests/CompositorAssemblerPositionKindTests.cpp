#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantRow.h>

#include <cstdio>
#include <string>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

static bool Contains(const std::string &text, const char *needle)
{
    return text.find(needle) != std::string::npos;
}

static MaterialVariantKey MakeBaseKey()
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Unlit;
    key.blend_mode = RenderAlphaMode::Opaque;
    key.pass_hint = PassType::ForwardOpaque;
    key.geometry_mode = GeometryMode::Mesh3D;
    return key;
}

static void CheckPositionKindForProvider(PositionProviderId provider, const char *expected_define)
{
    CompositorAssembler assembler;
    MaterialVariantKey key = MakeBaseKey();
    MaterialVariantRow row{};

    key.position_provider = provider;

    row.name = "PositionKindTestRow";
    row.surface_type = key.surface_type;
    row.geometry_mode = key.geometry_mode;
    row.position_provider = provider;
    row.blend = key.blend_mode;
    row.pass = key.pass_hint;

    MaterialVariantDesc desc = MaterialVariantDesc::CreateRowBound("PositionKindTestRow", &row);

    const auto result = assembler.AssembleVertexShader(key, desc);

    CHECK_TRUE(result.success);
    if (!result.success)
    {
        std::fprintf(stderr, "AssembleVertexShader failed: %s\n", result.error_message.c_str());
        return;
    }

    CHECK_TRUE(Contains(result.glsl, expected_define));
}

static void test_position_kind_mapping()
{
    CheckPositionKindForProvider(PositionProviderId::DirectVec3, "#define POSITION_KIND 2");
    CheckPositionKindForProvider(PositionProviderId::VAB_Vec2, "#define POSITION_KIND 1");
    CheckPositionKindForProvider(PositionProviderId::VAB_IVec2, "#define POSITION_KIND 1");
    CheckPositionKindForProvider(PositionProviderId::VAB_UVec2, "#define POSITION_KIND 1");
    CheckPositionKindForProvider(PositionProviderId::PCG_FullscreenTriangle, "#define POSITION_KIND 0");
}

int main()
{
    test_position_kind_mapping();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All CompositorAssemblerPositionKind tests passed.\n");
    return 0;
}
