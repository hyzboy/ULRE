#include <hgl/graph/module/ShaderGenContractPathContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/shadergen/MaterialCreateInfo.h>

#include <cstdio>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    mtl::MaterialCreateConfig cfg(PrimitiveType::Triangles, false);
    mtl::MaterialCreateInfo mci(&cfg);

    GraphicsContext gc(nullptr, ShaderGenPathMode::MirrorPreferred);

    ShaderGenContractPathContext ctx;
    BuildShaderGenContractPathContextWithBuilders(
        ctx,
        &gc,
        mci,
        "CtxStrictPrebuild",
        [](const mtl::MaterialCreateInfo &,
           mtl::contract::ShaderGenRequest &,
           const char *) -> bool
        {
            return true;
        },
        [](const mtl::MaterialCreateInfo &,
           mtl::contract::ShaderGenResult &) -> bool
        {
            return false;
        });

    if (ctx.mode != ShaderGenPathMode::MirrorPreferred)
    {
        std::fprintf(stderr, "[FAIL] mode should be mirror-preferred\n");
        ++failed;
    }

    if (!ctx.policy.require_mirror_valid)
    {
        std::fprintf(stderr, "[FAIL] mirror-preferred should require mirror-valid\n");
        ++failed;
    }

    if (ctx.request == nullptr)
    {
        std::fprintf(stderr, "[FAIL] request should be available when request builder succeeds\n");
        ++failed;
    }

    if (ctx.mirror != nullptr)
    {
        std::fprintf(stderr, "[FAIL] mirror should be null when result builder fails\n");
        ++failed;
    }

    if (!ctx.mirror_prebuild_failed)
    {
        std::fprintf(stderr, "[FAIL] mirror_prebuild_failed should be true when result builder fails\n");
        ++failed;
    }

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenContractPathContextTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenContractPathContextTest PASSED\n");
    return 0;
}
