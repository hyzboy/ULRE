/// FixedDefFactory3D.cpp — 通用 3D 工厂函数实现

#include"FixedDefFactory3D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/ShaderRequireScanner.h>
#include<hgl/shadergen/ShaderGenPathConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const FixedMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg)
{
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[%s] VariantRegistry lookup failed\n", debug_tag);
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
            debug_tag, result.error_message.c_str());
        return nullptr;
    }

    ShaderAutoRequirements auto_requirements;
    std::string require_diagnostics;
    const bool require_ok = CollectShaderAutoRequirements(def,
                                                          GetShaderLibraryPath(),
                                                          result.vertex_glsl,
                                                          result.fragment_glsl,
                                                          auto_requirements,
                                                          &require_diagnostics);
    if (!require_ok)
    {
        std::fprintf(stderr, "[%s] reflection collection failed:\n%s",
                     debug_tag,
                     require_diagnostics.c_str());
        return nullptr;
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

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        merged_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag);

    return mci;
}

}//namespace hgl::graph::mtl
