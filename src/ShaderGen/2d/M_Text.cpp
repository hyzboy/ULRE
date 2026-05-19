#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/MaterialResourceManifest.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

static MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,
                                        const Text2DMaterialCreateConfig *cfg,
                                        const MaterialVariantDesc &desc,
                                        const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return(nullptr);

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.position_format=VAT_IVEC2;

    // Build preambles (prepended before artifact GLSL)
    auto vs_preamble = build2d::Build2DVertexPreamble(&new_cfg, true, true, SamplerSlot::Text);
    auto fs_preamble = build2d::Build2DFragmentPreamble(&new_cfg, true, true, SamplerSlot::Text);

    // Populate vertex attrib key bits
    MaterialVariantKey assemble_key = key;

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg);
    vertices.push_back({VAT_VEC2, VAN::TexCoord});

    for (const auto &ve : vertices)
        assemble_key.SetVertexAttribEnabled(ve.attrib);

    CompositorAssembler assembler;

    // Use artifact path to collect SFM requirements alongside GLSL generation
    auto vs_artifact = assembler.AssembleVertexArtifact(assemble_key, desc,
                                                        nullptr,
                                                        new_cfg.coordinate_system);
    if (!vs_artifact.success)
    {
        std::fprintf(stderr, "[Text2D] CompositorAssembler VS failed: %s\n",
                     vs_artifact.error_message.c_str());
        return nullptr;
    }

    auto fs_artifact = assembler.AssembleFragmentArtifact(assemble_key, desc);
    if (!fs_artifact.success)
    {
        std::fprintf(stderr, "[Text2D] CompositorAssembler FS failed: %s\n",
                     fs_artifact.error_message.c_str());
        return nullptr;
    }

    MaterialResourceManifest manifest;
    AddTextureSampler(manifest.samplers, SamplerSlot::Text, SamplerType::Sampler2D);
    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "Text2D",
                                 &new_cfg,
                                 vertices,
                                 manifest,
                                 ShaderDataSchema::TextColor);

    // Merge VS + FS requirement sets, then combine with explicit manifest declarations.
    vs_artifact.req_set.MergeFrom(fs_artifact.req_set);
    MaterialResourceManifest merged_manifest = MaterialResourceManifest::FromStaticDef(def);
    merged_manifest.MergeKeepFirst(vs_artifact.req_set.ToManifest());

    StaticMaterialDef merged_def = merged_manifest.ProjectIntoStaticDef(def);

    std::string vs = vs_preamble + vs_artifact.glsl;
    std::string fs = fs_preamble + fs_artifact.glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile,
                                                        merged_def,
                                                        vs,
                                                        fs,
                                                        static_cast<const MaterialCreateConfig *>(&new_cfg));
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}

static MaterialCreateInfo *Text2D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{
    // cfg may arrive as Material3DCreateConfig* (converted from Text2DMaterialCreateConfig
    // by ShaderMaterialProgramManager::CreateMaterial) or as the original Text2DMaterialCreateConfig*.
    // Build a canonical Text2DMaterialCreateConfig from whichever arrived so that
    // CreateText2D always sees a correctly-typed config with the right coordinate_system.
    Text2DMaterialCreateConfig canonical;
    if (cfg)
    {
        if (cfg->kind == ConfigKind::Text2D)
        {
            // Correct type — use directly.
            canonical = *static_cast<Text2DMaterialCreateConfig *>(cfg);
        }
        else if (cfg->kind == ConfigKind::D3)
        {
            // Converted by the ShaderMaterialProgramManager bridge; extract coord_2d.
            const auto *cfg3d = static_cast<const Material3DCreateConfig *>(cfg);
            canonical.coordinate_system = cfg3d->coord_2d;
            canonical.material_instance = cfg->material_instance;
            canonical.texture_source_bits_override  = cfg->texture_source_bits_override;
            canonical.sampler_feature_bits_override = cfg->sampler_feature_bits_override;
        }
        // else: ConfigKind::D2 — use defaults (already set by Text2DMaterialCreateConfig ctor)
    }
    return CreateText2D(profile, &canonical, *desc, key);
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Text2D, "Text2D", hgl::graph::mtl::Text2D_Adapter)

