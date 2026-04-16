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
    constexpr FixedVertexEntry BILLBOARD_DYNAMIC_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const UBOSemanticSet BILLBOARD_DYNAMIC_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };

    const SSBOSemanticSet BILLBOARD_DYNAMIC_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialInstanceID,
        SSBODescriptorSemantic::MaterialInstanceData,
    };

    constexpr SamplerSlot BILLBOARD_DYNAMIC_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_DYNAMIC_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS) / sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS[0]));

    const StaticMaterialDef BILLBOARD_DYNAMIC_DEF_TEMPLATE {
        "BillboardDynamic",
        PrimitiveType::Triangles,
        BILLBOARD_DYNAMIC_VERTEX,
        uint32_t(sizeof(BILLBOARD_DYNAMIC_VERTEX) / sizeof(BILLBOARD_DYNAMIC_VERTEX[0])),
        &BILLBOARD_DYNAMIC_BASE_UBOS,
        &BILLBOARD_DYNAMIC_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::BillboardSizeUVec2,
    };
}//namespace

MaterialCreateInfo *CreateBillboard2DDynamic(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;
    cfg->material_instance=true;

    const bool use_array = cfg->use_texture_array;

    StaticTextureSamplerDescriptors dynamic_samplers;
    for (uint32_t i = 0; i < BILLBOARD_DYNAMIC_TEX_SLOT_COUNT; ++i)
        AddTextureSampler(dynamic_samplers, BILLBOARD_DYNAMIC_TEX_SLOTS[i],
                               use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                               0, 0,
                               cfg->base_color_channel);

    // When using texture arrays, the MIT SSBO carries per-instance layer indices.
    SSBOSemanticSet dynamic_ssbos = BILLBOARD_DYNAMIC_BASE_SSBOS;
    if (use_array)
        AddSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID);

    StaticMaterialDef dynamic_def = BILLBOARD_DYNAMIC_DEF_TEMPLATE;
    dynamic_def.texture_samplers  = &dynamic_samplers;
    dynamic_def.ssbo_descriptors  = &dynamic_ssbos;

    auto BlendModeToPassHint = [](RenderAlphaMode bm) -> PassType {
        switch (bm) {
        case RenderAlphaMode::Masked:          return PassType::ForwardMasked;
        case RenderAlphaMode::Dither:          return PassType::ForwardDither;
        case RenderAlphaMode::Opaque:          return PassType::ForwardOpaque;
        case RenderAlphaMode::AlphaToCoverage: return PassType::ForwardA2C;
        default:                         return PassType::ForwardTransparent;
        }
    };

    MaterialVariantKey var_key = build3d::MakeBillboardKeyBase(cfg->blend_mode);
    var_key.geometry_mode = GeometryMode::BillboardCameraFacing;

    std::fprintf(stderr, "[BillboardDynamic] use_array=%d  blend=%d  samplerType=%s\n",
        (int)use_array, (int)cfg->blend_mode,
        use_array ? "Sampler2DArray" : "Sampler2D");

    // Lookup with Simple key (registry only has Simple variants)
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardDynamic] VariantRegistry lookup failed\n");
        return nullptr;
    }

    // After lookup, switch to Array so CompositorAssembler emits TEXTURE_ARRAY_MODE
    if (use_array)
        var_key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Array);

    std::fprintf(stderr, "[BillboardDynamic] variant found: %s, assembling...\n",
        var_desc->variant_name.c_str());

    CompositorAssembler assembler;

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardDynamic] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    std::fprintf(stderr, "[BillboardDynamic] assemble OK, compiling material...\n");

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[BillboardDynamic] CompileCompositorMaterial failed\n");
    else
        std::fprintf(stderr, "[BillboardDynamic] material created OK\n");
    return mci;
}
}//namespace hgl::graph::mtl

