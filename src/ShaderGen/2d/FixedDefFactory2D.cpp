#include"FixedDefFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/ShaderRequireScanner.h>
#include<hgl/shadergen/ShaderGenPathConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

// Shared 2D compile path for low-variance creators.
// Text2D and other highly specialized creators should keep dedicated code paths.
MaterialCreateInfo *CreateFromFixedDef2D(const char *debug_tag,
                                         const contract::PhysicalDeviceProfileLite *profile,
                                         const FixedMaterialDef &def,
                                         const MaterialVariantKey &var_key,
                                         const std::string &vs_preamble,
                                         const std::string &fs_preamble,
                                         const Material2DCreateConfig *cfg,
                                         const bool use_canonical_fallback)
{
    if(!profile||!cfg)
        return(nullptr);

    // Canonical fallback is useful when caller builds keys with runtime overrides
    // (for example texture source mode overrides) and wants registry fallback behavior.
    const MaterialVariantDesc *var_desc = use_canonical_fallback
        ? GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(var_key,nullptr)
        : GetBuiltinVariantRegistry().QueryVariant(var_key);

    if(!var_desc)
    {
        std::fprintf(stderr, "[%s] VariantRegistry lookup failed\n", debug_tag ? debug_tag : "2DFactory");
        return nullptr;
    }

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(var_key, *var_desc);
    if(!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
                     debug_tag ? debug_tag : "2DFactory",
                     result.error_message.c_str());
        return nullptr;
    }

    const std::string vs = vs_preamble + result.vertex_glsl;
    const std::string fs = fs_preamble + result.fragment_glsl;

    ShaderAutoRequirements auto_requirements;
    std::string require_diagnostics;
    const bool require_ok = CollectShaderAutoRequirements(GetShaderLibraryPath(),
                                                          vs,
                                                          fs,
                                                          auto_requirements,
                                                          &require_diagnostics);
    if (!require_ok && !require_diagnostics.empty())
    {
        std::fprintf(stderr, "[%s] @require scan warnings:\n%s",
                     debug_tag ? debug_tag : "2DFactory",
                     require_diagnostics.c_str());
    }

    FixedUBODescriptors merged_ubos;
    FixedSSBODescriptors merged_ssbos;
    FixedTextureSamplerDescriptors merged_samplers;
    FixedMaterialDef merged_def = def;
    MergeShaderAutoRequirements(def,
                                auto_requirements,
                                merged_def,
                                merged_ubos,
                                merged_ssbos,
                                merged_samplers);

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, merged_def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag ? debug_tag : "2DFactory");

    return mci;
}

}//namespace hgl::graph::mtl
