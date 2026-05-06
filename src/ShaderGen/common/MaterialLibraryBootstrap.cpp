#include "MaterialLibraryBootstrap.h"
#include "VariantLookupService.h"

#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/shadergen/MaterialFactory3D.h>
#include <hgl/shadergen/GLSLCompilerConfig.h>

#include "../BuiltinVariantEntry.h"

#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace hgl::graph::mtl::bootstrap
{

namespace
{

std::string ResolveShaderLibraryPathFromContext()
{
    const ShaderCompilerContext shader_context = CaptureShaderCompilerContext();

    if(!shader_context.shader_library_path.empty())
        return shader_context.shader_library_path;

    return "ShaderLibrary";
}

bool RunStartupVariantValidation()
{
    std::vector<std::string> diagnostics;

    const bool ok = GetBuiltinVariantRegistry().ValidateBuiltinVariantTemplates(
        ResolveShaderLibraryPathFromContext(),
        diagnostics);

    if (ok)
    {
        std::printf("[MaterialLibrary] Startup variant validation passed.\n");
        return true;
    }

    std::fprintf(stderr,
                 "[MaterialLibrary] Startup variant validation failed: %zu issue(s).\n",
                 diagnostics.size());

    for (const auto &msg : diagnostics)
        std::fprintf(stderr, "[MaterialLibrary] %s\n", msg.c_str());

    return false;
}

void RunRoutingConsistencySelfTest()
{
    bool all_ok = true;

    for (size_t i = 0; i < kBuiltinVariantsCount; ++i)
    {
        const auto &e = kBuiltinVariants[i];
        const MaterialVariantKey k = BuildKey(e);
        routing::VariantLookupResult lookup_result{};
        const bool lookup_ok = routing::ResolveBuiltinVariantForKey(k, lookup_result);
        const MaterialVariantDesc *found = lookup_ok ? lookup_result.variant_desc : nullptr;

        const bool entry_ok = found
                           && found->factory_type.has_value()
                           && *found->factory_type == e.preset;

        if (!entry_ok)
        {
            std::fprintf(stderr,
                         "[MaterialLibrary] FATAL: routing self-test FAILED for entry[%zu] \"%s\""
                         " (preset=%u): registry returned %s\n",
                         i,
                         e.name,
                         static_cast<unsigned>(e.preset),
                         found ? (found->factory_type.has_value()
                                      ? "wrong factory_type"
                                      : "desc with no factory_type")
                               : "nullptr");
            all_ok = false;
        }
    }

    if (!all_ok)
    {
        std::fprintf(stderr,
                     "[MaterialLibrary] FATAL: BuiltinVariantEntry routing self-test failed"
                     " - aborting to prevent undefined behaviour in main loop.\n");
        std::abort();
    }

    std::printf("[MaterialLibrary] BuiltinVariantEntry routing self-test passed"
                " (%zu entries).\n",
                kBuiltinVariantsCount);
}

void RunMaterialLibraryBootstrap()
{
    MaterialFactory3D::RegisterBuiltinFactories();
    (void)RunStartupVariantValidation();
    RunRoutingConsistencySelfTest();
}

} // namespace

void EnsureMaterialLibraryBootstrap()
{
    static std::once_flag once;
    std::call_once(once, []()
    {
        RunMaterialLibraryBootstrap();
    });
}

} // namespace hgl::graph::mtl::bootstrap
