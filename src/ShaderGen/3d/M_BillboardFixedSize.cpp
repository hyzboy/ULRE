#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<cstdio>
#include<vector>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include"Build3DCommon.h"

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry BILLBOARD_FIXED_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    // Non-texture descriptors �?texture entries built dynamically.
    const UBOSemanticSet BILLBOARD_FIXED_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };

    const SSBOSemanticSet BILLBOARD_FIXED_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialInstanceID,
        SSBODescriptorSemantic::MaterialInstanceData,
    };

    constexpr SamplerSlot BILLBOARD_FIXED_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_FIXED_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_FIXED_TEX_SLOTS) / sizeof(BILLBOARD_FIXED_TEX_SLOTS[0]));

    const StaticMaterialDef BILLBOARD_FIXED_DEF_TEMPLATE {
        "BillboardFixed",
        PrimitiveType::Triangles,
        BILLBOARD_FIXED_VERTEX,
        uint32_t(sizeof(BILLBOARD_FIXED_VERTEX) / sizeof(BILLBOARD_FIXED_VERTEX[0])),
        &BILLBOARD_FIXED_BASE_UBOS,
        &BILLBOARD_FIXED_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::BillboardSizeUVec2,
    };
}//namespace

MaterialCreateInfo *CreateBillboard2DFixed(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;
    cfg->material_instance=true;

    const bool use_array = cfg->use_texture_array;

    StaticTextureSamplerDescriptors dynamic_samplers;
    for (uint32_t i = 0; i < BILLBOARD_FIXED_TEX_SLOT_COUNT; ++i)
        AddTextureSampler(dynamic_samplers, BILLBOARD_FIXED_TEX_SLOTS[i],
                               use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                               0, 0,
                               cfg->base_color_channel);

    // When using texture arrays, the MIT SSBO carries per-instance layer indices.
    SSBOSemanticSet dynamic_ssbos = BILLBOARD_FIXED_BASE_SSBOS;
    if (use_array)
        AddSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID);

    StaticMaterialDef dynamic_def = BILLBOARD_FIXED_DEF_TEMPLATE;
    dynamic_def.texture_samplers  = &dynamic_samplers;
    dynamic_def.ssbo_descriptors  = &dynamic_ssbos;

    MaterialVariantKey lookup_key = build3d::MakeBillboardKeyBase(cfg->blend_mode);
    lookup_key.geometry_mode = GeometryMode::BillboardAxisLocked;

    MaterialVariantKey assemble_key = lookup_key;
    if (use_array)
        assemble_key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Array);

    std::fprintf(stderr, "[BillboardFixed] use_array=%d  blend=%d  samplerType=%s\n",
        (int)use_array, (int)cfg->blend_mode,
        use_array ? "Sampler2DArray" : "Sampler2D");

    // Registry normalizes billboard variants to the Simple lookup key. Array mode only
    // affects final shader assembly (TEXTURE_ARRAY_MODE + sampler2DArray emission).
    MaterialVariantKey resolved_lookup_key{};
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(lookup_key, &resolved_lookup_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardFixed] VariantRegistry lookup failed\n");
        return nullptr;
    }

    std::fprintf(stderr,
        "[BillboardFixed] variant found: %s, lookup_hash=%llu resolved_lookup_hash=%llu assemble_hash=%llu\n",
        var_desc->variant_name.c_str(),
        static_cast<unsigned long long>(lookup_key.Hash()),
        static_cast<unsigned long long>(resolved_lookup_key.Hash()),
        static_cast<unsigned long long>(assemble_key.Hash()));

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardFixed] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    std::fprintf(stderr, "[BillboardFixed] assemble OK, compiling material...\n");

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[BillboardFixed] CompileCompositorMaterial failed\n");
    else
        std::fprintf(stderr, "[BillboardFixed] material created OK\n");
    return mci;
}
}//namespace hgl::graph::mtl

