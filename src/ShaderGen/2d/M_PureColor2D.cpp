#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/math/Vector.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    constexpr const char mi_codes[]="vec4 Color;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4f);

    cfg->material_instance=true;

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, true);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, true);

    const MaterialVariantKey var_key = MapPresetToVariantKey(MaterialPreset::PureColor2D);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[PureColor2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(var_key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[PureColor2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    // Build DEF dynamically
    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    build2d::PushBaseUBODescriptors(ubos, cfg);
    build2d::PushBaseSSBODescriptors(ssbos, cfg);

    FixedMaterialDef def {
        "PureColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        nullptr,
        mi_codes, mi_bytes,
    };

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[PureColor2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
