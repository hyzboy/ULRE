#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderRequireScanner.h>
#include <hgl/shadergen/ShaderGenPathConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    // MI layout — same whether slots are sampler2D or sampler2DArray.
    // Layer indices for Array slots are stored separately in the MIT SSBO (MaterialInstanceTextureID).
    constexpr const char mi_codes_simple[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
    )";
    constexpr const uint32_t mi_bytes_simple = sizeof(uint32_t) + sizeof(float) * 3;

    constexpr FixedVertexEntry STANDARD_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord },
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Normal },
    };

    // Non-texture descriptors only �?texture entries are built dynamically in CreateStandardVariant().
    const FixedUBODescriptors STANDARD_BASE_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::SkyInfo,      uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors STANDARD_BASE_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    // Ordered list of texture slots used by the Standard material.
    // Texture sampler descriptors are built from this list at variant-creation time.
    constexpr SamplerSlot STANDARD_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
        SamplerSlot::Normal,
    };
    constexpr uint32_t STANDARD_TEX_SLOT_COUNT = uint32_t(sizeof(STANDARD_TEX_SLOTS) / sizeof(STANDARD_TEX_SLOTS[0]));

    const FixedMaterialDef STANDARD_DEF_TEMPLATE {
        "Standard_v1",
        PrimitiveType::Triangles,
        STANDARD_VERTEX,
        uint32_t(sizeof(STANDARD_VERTEX) / sizeof(STANDARD_VERTEX[0])),
        &STANDARD_BASE_UBOS,
        &STANDARD_BASE_SSBOS,
        nullptr,
        mi_codes_simple,
        mi_bytes_simple,
    };

} // anonymous namespace

MaterialCreateInfo *CreateStandardVariant(const contract::PhysicalDeviceProfileLite *profile,
                                          const MaterialVariantKey &input_key,
                                          const Material3DCreateConfig *cfg)
{
    if (!cfg)
    {
        std::fprintf(stderr, "[Standard] CreateStandardVariant failed: cfg is null\n");
        return nullptr;
    }

    if (!profile)
    {
        std::fprintf(stderr, "[Standard] CreateStandardVariant warning: profile is null\n");
    }

    const TextureSourceMode base_mode = input_key.GetTextureSourceMode(SamplerSlot::BaseColor);
    const TextureSourceMode normal_mode = input_key.GetTextureSourceMode(SamplerSlot::Normal);
    const bool has_per_slot_mode = input_key.HasAnyTextureSourceBits();
    const TextureSourceMode legacy_mode = input_key.GetPrimaryTextureSourceMode();

    const TextureSourceMode resolved_base = has_per_slot_mode ? base_mode : legacy_mode;
    const TextureSourceMode resolved_normal = has_per_slot_mode ? normal_mode : legacy_mode;

    const bool base_is_array = resolved_base == TextureSourceMode::Array;
    const bool normal_is_array = resolved_normal == TextureSourceMode::Array;
    const bool any_array = base_is_array || normal_is_array;

    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    SkyLightAmbientModel ambient = cfg_with_mi.sky_ambient_model;

    // Start with stable non-texture descriptors, then append texture entries.
    FixedSSBODescriptors dynamic_ssbos = STANDARD_BASE_SSBOS;
    FixedTextureSamplerDescriptors dynamic_samplers;

    // Per-slot resolved modes, in the same order as STANDARD_TEX_SLOTS.
    const TextureSourceMode tex_slot_modes[STANDARD_TEX_SLOT_COUNT] = {
        resolved_base, resolved_normal
    };
    for (uint32_t i = 0; i < STANDARD_TEX_SLOT_COUNT; ++i)
        AddFixedTextureSampler(dynamic_samplers,
                               STANDARD_TEX_SLOTS[i],
                               uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                               tex_slot_modes[i] == TextureSourceMode::Array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);

    // When any slot is Array the MIT SSBO provides per-instance layer indices.
    if (any_array)
        AddFixedSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_samplers,
        unused_resources);

    FixedMaterialDef dynamic_def = STANDARD_DEF_TEMPLATE;
    dynamic_def.ssbo_descriptors        = &dynamic_ssbos;
    dynamic_def.texture_samplers        = &dynamic_samplers;
    dynamic_def.mi_glsl_codes           = mi_codes_simple;
    dynamic_def.mi_struct_bytes         = mi_bytes_simple;
    dynamic_def.name                    = any_array ? "StandardTextureArray_v1" : "Standard_v1";

    MaterialVariantKey route_key = input_key;
    route_key.surface_type        = SurfaceType::Standard;
    route_key.texture_source_mode = any_array ? TextureSourceMode::Array : TextureSourceMode::Simple;
    route_key.texture_source_bits = 0;
    route_key.sampler_feature_bits = SamplerFeatureBit(SamplerSlot::BaseColor)
                                   | SamplerFeatureBit(SamplerSlot::Normal);

    MaterialVariantKey assemble_key = route_key;
    if (has_per_slot_mode)
    {
        assemble_key.SetTextureSourceMode(SamplerSlot::BaseColor, resolved_base);
        assemble_key.SetTextureSourceMode(SamplerSlot::Normal,    resolved_normal);
    }
    else
    {
        assemble_key.SetTextureSourceMode(SamplerSlot::BaseColor, route_key.texture_source_mode);
        assemble_key.SetTextureSourceMode(SamplerSlot::Normal,    route_key.texture_source_mode);
    }

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(route_key);
    if (!var_desc)
    {
        std::fprintf(stderr,
            "[Standard] VariantRegistry lookup failed (route_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X any_array=%d)\n",
            static_cast<unsigned long long>(route_key.Hash()),
            static_cast<unsigned>(route_key.surface_type),
            static_cast<unsigned>(route_key.geometry_mode),
            static_cast<unsigned>(route_key.texture_source_mode),
            route_key.texture_source_bits,
            route_key.sampler_feature_bits,
            route_key.vertex_attribute_feature_bits,
            route_key.extra_feature_bits,
            any_array ? 1 : 0);
        return nullptr;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    ShaderAutoRequirements auto_requirements;
    std::string require_diagnostics;
    const bool require_ok = CollectShaderAutoRequirements(GetShaderLibraryPath(),
                                                          result.vertex_glsl,
                                                          result.fragment_glsl,
                                                          auto_requirements,
                                                          &require_diagnostics);
    if (!require_ok && !require_diagnostics.empty())
        std::fprintf(stderr, "[Standard] @require scan warnings:\n%s", require_diagnostics.c_str());

    FixedUBODescriptors merged_ubos;
    FixedSSBODescriptors merged_ssbos;
    FixedTextureSamplerDescriptors merged_samplers;
    FixedMaterialDef merged_def = dynamic_def;
    MergeShaderAutoRequirements(dynamic_def,
                                auto_requirements,
                                merged_def,
                                merged_ubos,
                                merged_ssbos,
                                merged_samplers);

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        merged_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[Standard] CompileCompositorMaterial failed\n");
    return mci;
}

// Unified factory �?TextureSourceMode::Simple  -> sampler2D  (classic single-texture Standard)
//                  TextureSourceMode::Array   -> sampler2DArray (texture-atlas / array Standard)
MaterialCreateInfo *CreateStandard(const contract::PhysicalDeviceProfileLite *profile,
                                   const Material3DCreateConfig *cfg,
                                   TextureSourceMode tex_source)
{
    MaterialVariantKey key;
    key.surface_type = SurfaceType::Standard;
    key.texture_source_mode = tex_source;
    return CreateStandardVariant(profile, key, cfg);
}

// Compat wrappers �?keep the two named entry-points so MaterialLibrary.cpp
// does not need to change its dispatch table.
MaterialCreateInfo *CreateStandard(const contract::PhysicalDeviceProfileLite *profile,
                                   const Material3DCreateConfig *cfg)
{
    return CreateStandard(profile, cfg, TextureSourceMode::Simple);
}

}//namespace hgl::graph::mtl
