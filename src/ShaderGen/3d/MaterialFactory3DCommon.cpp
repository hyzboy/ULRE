/// MaterialFactory3D.cpp — 通用 3D 工厂函数实现

#include"MaterialFactory3DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg)
{
    if (cfg && cfg->prim != def.primitive_type)
    {
        std::fprintf(stderr,
            "[%s] Primitive mismatch: cfg->prim=%u def.primitive_type=%u (using def value)\n",
            debug_tag ? debug_tag : "3DFactory",
            static_cast<unsigned>(cfg->prim),
            static_cast<unsigned>(def.primitive_type));
    }

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[%s] VariantRegistry lookup failed\n", debug_tag);
        return nullptr;
    }

    // Populate vertex attribute feature bits from the actual vertex layout.
    // Builder functions (e.g. BuildForwardLitVS) derive has_uv0 / has_normal etc. from these bits.
    MaterialVariantKey assemble_key = var_key;
    PopulateVariantKeyVertexAttribBits(assemble_key, def);

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
            debug_tag, result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag);

    return mci;
}

}//namespace hgl::graph::mtl
