/// MaterialFactory3D.cpp — 通用 3D 工厂函数实现

#include"MaterialFactory3DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/GLSLCompilerConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>

namespace hgl::graph::mtl{

std::unique_ptr<MaterialCreateInfo> CreateFromFixedDef3DOwned(
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
            "[%s] Primitive mismatch: cfg->prim=%u def.primitive_type=%u (using def value)\n",
            debug_tag ? debug_tag : "3DFactory",
            static_cast<unsigned>(cfg->prim),
            static_cast<unsigned>(def.primitive_type));
    }

    // Populate vertex attribute feature bits from the actual vertex layout.
    // Builder functions (e.g. BuildForwardLitVS) derive has_uv0 / has_normal etc. from these bits.
    MaterialVariantKey assemble_key = var_key;
    PopulateVariantKeyVertexAttribBits(assemble_key, def);

    const ShaderCompilerContext shader_context = CaptureShaderCompilerContext();
    CompositorAssembler assembler(shader_context.shader_library_path);

    auto result = assembler.Assemble(assemble_key, var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
            debug_tag, result.error_message.c_str());
        return nullptr;
    }

    auto mci = CompileCompositorMaterialOwned(
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
