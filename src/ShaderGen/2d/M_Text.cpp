#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,
                                   const Text2DMaterialCreateConfig *cfg,
                                   const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return(nullptr);

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(key);
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
    const auto result = assembler.Assemble(key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[Text2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg);
    vertices.push_back({VAT_VEC2, VAN::TexCoord});

    StaticMaterialDef def{};
    UBOSemanticSet ubos;
    SSBOSemanticSet ssbos;
    StaticTextureSamplerDescriptors samplers;
    AddTextureSampler(samplers, SamplerSlot::Text, SamplerType::Sampler2D);
    build2d::BuildBase2DFixedDef(def,
                                 "Text2D",
                                 &new_cfg,
                                 vertices,
                                 ubos,
                                 ssbos,
                                 &samplers,
                                 InstanceDataLayout::Text2D);

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &new_cfg);
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

