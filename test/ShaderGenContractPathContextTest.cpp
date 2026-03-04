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

    mtl::contract::PhysicalDeviceProfileLite profile;
    profile.name = "InjectedProfile";
    profile.api_version = 1;
    profile.limits.max_vertex_input_attributes = 8;

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
        },
        &profile);

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
    else
    {
        if (!ctx.request->has_physical_device_profile)
        {
            std::fprintf(stderr, "[FAIL] request should be injected with preferred profile\n");
            ++failed;
        }
        else if (ctx.request->physical_device_profile.name != "InjectedProfile")
        {
            std::fprintf(stderr, "[FAIL] injected profile name mismatch\n");
            ++failed;
        }
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
