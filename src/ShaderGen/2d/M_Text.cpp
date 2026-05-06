#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>
#include <memory>

namespace hgl::graph::mtl{

std::unique_ptr<MaterialCreateInfo> CreateText2DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                      const Text2DMaterialCreateConfig *cfg,
                                                      const MaterialVariantDesc &desc,
                                                      const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return nullptr;

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.position_format=VAT_IVEC2;
    new_cfg.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto vs_preamble = build2d::Build2DVertexPreamble(&new_cfg, true, true, SamplerSlot::Text);
    auto fs_preamble = build2d::Build2DFragmentPreamble(&new_cfg, true, true, SamplerSlot::Text);

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(key, desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[Text2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg);
    vertices.push_back({VAT_VEC2, VAN::TexCoord});

    MaterialResourceManifest manifest;
    AddTextureSampler(manifest.samplers, SamplerSlot::Text, SamplerType::Sampler2D);
    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "Text2D",
                                 &new_cfg,
                                 vertices,
                                 manifest,
                                 ShaderDataSchema::TextColor);

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    auto mci = CompileCompositorMaterialOwned(profile, def, vs, fs, &new_cfg);
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(
    Text2D,
    const hgl::graph::mtl::Text2DMaterialCreateConfig)

