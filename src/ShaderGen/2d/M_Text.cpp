#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,const Text2DMaterialCreateConfig *cfg)
{
    constexpr const char mi_codes[]="uint TextColor;";
    constexpr const uint32_t mi_bytes=sizeof(uint32);
    if(!profile||!cfg)
        return(nullptr);

    const MaterialVariantKey var_key = MapPresetToVariantKey(MaterialPreset::Text2D);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[Text2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.position_format=VAT_IVEC2;
    new_cfg.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto vs_preamble = build2d::Build2DVertexPreamble(&new_cfg, true, true, SamplerSlot::Text);
    auto fs_preamble = build2d::Build2DFragmentPreamble(&new_cfg, true, true, SamplerSlot::Text);

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(var_key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[Text2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg);
    vertices.push_back({VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord});

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    FixedTextureSamplerDescriptors samplers;
    build2d::PushBaseUBODescriptors(ubos, &new_cfg);
    build2d::PushBaseSSBODescriptors(ssbos, &new_cfg);
    AddFixedTextureSampler(samplers, SamplerSlot::Text, SamplerType::Sampler2D);

    FixedMaterialDef def {
        "Text2D",
        new_cfg.prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        &samplers,
        mi_codes, mi_bytes,
    };

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &new_cfg);
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

