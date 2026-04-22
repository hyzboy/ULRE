/// MaterialFactory3D.cpp — 通用 3D 工厂函数实现

#include"MaterialFactory3DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateFromFixedDef3D(
    const char *debug_tag,
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const MaterialVariantKey &var_key,
    const Material3DCreateConfig *cfg)
{
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
