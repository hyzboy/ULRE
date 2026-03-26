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
    constexpr const char mi_codes[]="uvec2 BillboardSize;";
    constexpr const uint32_t mi_bytes=sizeof(uint32_t)*2;       // uvec2 = 2 x uint32

    constexpr FixedVertexEntry BILLBOARD_FIXED_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
    };

    // Non-texture descriptors �?texture entries built dynamically.
    const FixedUBODescriptors BILLBOARD_FIXED_BASE_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors BILLBOARD_FIXED_BASE_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
    };

    constexpr SamplerSlot BILLBOARD_FIXED_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t BILLBOARD_FIXED_TEX_SLOT_COUNT = uint32_t(sizeof(BILLBOARD_FIXED_TEX_SLOTS) / sizeof(BILLBOARD_FIXED_TEX_SLOTS[0]));

    const FixedMaterialDef BILLBOARD_FIXED_DEF_TEMPLATE {
        "BillboardFixed",
        PrimitiveType::Triangles,
        BILLBOARD_FIXED_VERTEX,
        uint32_t(sizeof(BILLBOARD_FIXED_VERTEX) / sizeof(BILLBOARD_FIXED_VERTEX[0])),
        &BILLBOARD_FIXED_BASE_UBOS,
        &BILLBOARD_FIXED_BASE_SSBOS,
        nullptr,
        mi_codes,
        mi_bytes,
    };
}//namespace

MaterialCreateInfo *CreateBillboard2DFixedSize(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;
    cfg->material_instance=true;

    FixedTextureSamplerDescriptors dynamic_samplers;
    for (uint32_t i = 0; i < BILLBOARD_FIXED_TEX_SLOT_COUNT; ++i)
        AddFixedTextureSampler(dynamic_samplers,
                               BILLBOARD_FIXED_TEX_SLOTS[i],
                               uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                               SamplerType::Sampler2D);

    FixedMaterialDef dynamic_def = BILLBOARD_FIXED_DEF_TEMPLATE;
    dynamic_def.texture_samplers = &dynamic_samplers;

    auto BlendModeToPassHint = [](BlendMode bm) -> PassType {
        switch (bm) {
        case BlendMode::Masked:  return PassType::ForwardMasked;
        case BlendMode::Dither:  return PassType::ForwardDither;
        case BlendMode::Opaque:  return PassType::ForwardOpaque;
        default:                 return PassType::ForwardTransparent;
        }
    };

    MaterialVariantKey var_key;
    var_key.geometry_mode       = GeometryMode::BillboardAxisLocked;
    var_key.texture_source_mode = TextureSourceMode::Simple;
    var_key.SetHasTexture(SamplerSlot::BaseColor);
    var_key.blend_mode          = cfg->blend_mode;
    var_key.pass_hint           = BlendModeToPassHint(cfg->blend_mode);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[BillboardFixed] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[BillboardFixed] CompositorAssembler failed: %s\n",
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
        std::fprintf(stderr, "[BillboardFixed] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
