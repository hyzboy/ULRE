#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/GLSLCompilerConfig.h>
#include<cstdio>

namespace hgl::graph::mtl{

// Shared 2D compile path for low-variance creators.
// Text2D and other highly specialized creators should keep dedicated code paths.
std::unique_ptr<MaterialCreateInfo> CreateFromFixedDef2DOwned(const char *debug_tag,
                                                              const contract::PhysicalDeviceProfileLite *profile,
                                                              const StaticMaterialDef &def,
                                                              const MaterialVariantKey &var_key,
                                                              const std::string &vs_preamble,
                                                              const std::string &fs_preamble,
                                                              const Material2DCreateConfig *cfg,
                                                              const MaterialVariantDesc &var_desc)
{
    if(!profile||!cfg)
        return nullptr;

    // Populate vertex attribute feature bits from the actual vertex layout.
    MaterialVariantKey assemble_key = var_key;
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        assemble_key.SetVertexAttribEnabled(def.vertex_entries[i].attrib);

    const ShaderCompilerContext shader_context = CaptureShaderCompilerContext();
    CompositorAssembler assembler(shader_context.shader_library_path);
    const auto result = assembler.Assemble(assemble_key, var_desc);
    if(!result.success)
    {
        std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
                     debug_tag ? debug_tag : "2DFactory",
                     result.error_message.c_str());
        return nullptr;
    }

    const std::string vs = vs_preamble + result.vertex_glsl;
    const std::string fs = fs_preamble + result.fragment_glsl;

    auto mci = CompileCompositorMaterialOwned(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", debug_tag ? debug_tag : "2DFactory");

    return mci;
}

}//namespace hgl::graph::mtl
