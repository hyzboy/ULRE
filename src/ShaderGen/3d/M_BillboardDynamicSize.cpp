#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>
#include<vector>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry BILLBOARD_DYNAMIC_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    constexpr SamplerSlot BILLBOARD_DYNAMIC_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_DYNAMIC_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS) / sizeof(BILLBOARD_DYNAMIC_TEX_SLOTS[0]));
}//namespace

MaterialCreateInfo *CreateBillboard2DDynamic(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;

    FixedUBODescriptors ubos = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };

    FixedSSBODescriptors ssbos = {
        SSBODescriptorSemantic::LocalToWorld,
        SSBODescriptorSemantic::TransformID,
    };

    FixedTextureSamplerDescriptors samplers;
    for (uint32_t i = 0; i < BILLBOARD_DYNAMIC_TEX_SLOT_COUNT; ++i)
        AddFixedTextureSampler(samplers, BILLBOARD_DYNAMIC_TEX_SLOTS[i], SamplerType::Sampler2D,
                               0, 0,
                               cfg->base_color_channel);

    FixedMaterialDef dynamic_def {
        "BillboardDynamic",
        PrimitiveType::Triangles,
        BILLBOARD_DYNAMIC_VERTEX,
        uint32_t(sizeof(BILLBOARD_DYNAMIC_VERTEX) / sizeof(BILLBOARD_DYNAMIC_VERTEX[0])),
        &ubos,
        &ssbos,
        &samplers,
        nullptr,
        0,
    };

    auto BlendModeToPassHint = [](BlendMode bm) -> PassType {
        switch (bm) {
        case BlendMode::Masked:  return PassType::ForwardMasked;
        case BlendMode::Dither:  return PassType::ForwardDither;
        case BlendMode::Opaque:  return PassType::ForwardOpaque;
        default:                 return PassType::ForwardTransparent;
        }
    };

    MaterialVariantKey var_key;
    var_key.geometry_mode       = GeometryMode::BillboardCameraFacing;
    var_key.texture_source_mode = TextureSourceMode::Simple;
    var_key.SetHasTexture(SamplerSlot::BaseColor);
    var_key.blend_mode          = cfg->blend_mode;
    var_key.pass_hint           = BlendModeToPassHint(cfg->blend_mode);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardDynamic] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardDynamic] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[BillboardDynamic] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

