/// MaterialFactory3D.cpp — 通用 3D 工厂函数实现

#include"MaterialFactory3DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/ErrorIndicatorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg,
    const MaterialVariantDesc &var_desc)
{
    if (cfg && cfg->prim != def.primitive_type)
    {
        std::fprintf(stderr,
            "[%s] Primitive mismatch: cfg->prim=%u def.primitive_type=%u (using cfg value)\n",
            debug_tag ? debug_tag : "3DFactory",
            static_cast<unsigned>(cfg->prim),
            static_cast<unsigned>(def.primitive_type));
    }

    StaticMaterialDef effective_def = def;
    if (cfg)
        effective_def.primitive_type = cfg->prim;

    // Populate vertex attribute feature bits from the actual vertex layout.
    // Builder functions (e.g. BuildForwardLitVS) derive has_uv0 / has_normal etc. from these bits.
    MaterialVariantKey assemble_key = var_key;
    PopulateVariantKeyVertexAttribBits(assemble_key, effective_def);

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
            debug_tag, result.error_message.c_str());
        return nullptr;
    }

    // Phase 2: if this variant was assembled with an ErrorIndicator FS override,
    // re-assemble the fragment shader via AssembleErrorIndicatorFS so that the
    // error_code is baked in as a compile-time constant.
    if (var_desc.fs_error_code != 0)
    {
        std::string ei_fs;
        std::string ei_err;
        AssembleErrorIndicatorFS(assemble_key, var_desc, var_desc.fs_error_code, ei_fs, ei_err);
        result.fragment_glsl = std::move(ei_fs);
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        effective_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag);

    return mci;
}

}//namespace hgl::graph::mtl
