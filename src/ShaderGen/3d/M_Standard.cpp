#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/RenderAssignDef.h>
#include <cstdio>
#include <vector>

#include "../common/MFSkyLight.h"
#include <hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    // MI layout for Simple (sampler2D) — no texture_id needed
    constexpr const char mi_codes_simple[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
    )";
    constexpr const uint32_t mi_bytes_simple = sizeof(uint32_t) + sizeof(float) * 3;

    // MI layout for Array (sampler2DArray) — texture_id selects the array layer
    constexpr const char mi_codes_array[] = R"(
        uint  base_color;
        float metallic;
        float roughness;
        float normal_scale;
        uint  texture_id;
    )";
    constexpr const uint32_t mi_bytes_array = sizeof(uint32_t) * 2 + sizeof(float) * 3;

    constexpr FixedVertexEntry STANDARD_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::ATTRIB },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::ATTRIB },
    };

    // Descriptor template — texture slots use sampler2D as placeholder;
    // CreateStandard() patches glsl_type at runtime based on TextureSourceMode.
    constexpr FixedDescriptorEntry STANDARD_DESCRIPTORS[] = {
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

    // Index of the first TextureSampler entry in STANDARD_DESCRIPTORS
    constexpr uint32_t STANDARD_TEX_SLOT_FIRST = 5;
    constexpr uint32_t STANDARD_TEX_SLOT_COUNT  = 3;

    constexpr FixedMaterialDef STANDARD_DEF_TEMPLATE {
        "Standard_v1",
        PrimitiveType::Triangles,
        STANDARD_VERTEX,
        uint32_t(sizeof(STANDARD_VERTEX) / sizeof(STANDARD_VERTEX[0])),
        STANDARD_DESCRIPTORS,
        uint32_t(sizeof(STANDARD_DESCRIPTORS) / sizeof(STANDARD_DESCRIPTORS[0])),
        mi_codes_simple,
        mi_bytes_simple,
    };

} // anonymous namespace

static const char *ToSamplerType(const TextureSourceMode mode)
{
    return mode == TextureSourceMode::Array ? "sampler2DArray" : "sampler2D";
}

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

    const TextureSourceMode base_mode = input_key.GetTextureSourceMode(TextureSlot::BaseColor);
    const TextureSourceMode normal_mode = input_key.GetTextureSourceMode(TextureSlot::Normal);
    const TextureSourceMode rough_mode = input_key.GetTextureSourceMode(TextureSlot::Roughness);

    const bool has_per_slot_mode = input_key.HasAnyTextureSourceBits();
    const TextureSourceMode legacy_mode = input_key.GetPrimaryTextureSourceMode();

    const TextureSourceMode resolved_base = has_per_slot_mode ? base_mode : legacy_mode;
    const TextureSourceMode resolved_normal = has_per_slot_mode ? normal_mode : legacy_mode;
    const TextureSourceMode resolved_rough = has_per_slot_mode ? rough_mode : legacy_mode;

    const bool base_is_array = resolved_base == TextureSourceMode::Array;
    const bool normal_is_array = resolved_normal == TextureSourceMode::Array;
    const bool rough_is_array = resolved_rough == TextureSourceMode::Array;
    const bool any_array = base_is_array || normal_is_array || rough_is_array;

    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    SkyLightAmbientModel ambient = cfg_with_mi.sky_ambient_model;

    // Build a mutable copy of the descriptor list so we can patch sampler types
    std::vector<FixedDescriptorEntry> dynamic_descriptors(
        STANDARD_DESCRIPTORS,
        STANDARD_DESCRIPTORS + uint32_t(sizeof(STANDARD_DESCRIPTORS) / sizeof(STANDARD_DESCRIPTORS[0])));

    // Patch texture slots independently.
    dynamic_descriptors[STANDARD_TEX_SLOT_FIRST + 0].glsl_type = ToSamplerType(resolved_base);
    dynamic_descriptors[STANDARD_TEX_SLOT_FIRST + 1].glsl_type = ToSamplerType(resolved_normal);
    dynamic_descriptors[STANDARD_TEX_SLOT_FIRST + 2].glsl_type = ToSamplerType(resolved_rough);

    std::vector<const char *> unused_resources;
    ApplySkyLightResourceInjection(
        GetSkyLightResourceInjectionSpec(ambient),
        dynamic_descriptors,
        unused_resources);

    FixedMaterialDef dynamic_def = STANDARD_DEF_TEMPLATE;
    dynamic_def.descriptor_entries      = dynamic_descriptors.data();
    dynamic_def.descriptor_entry_count  = uint32_t(dynamic_descriptors.size());
    dynamic_def.mi_glsl_codes           = any_array ? mi_codes_array : mi_codes_simple;
    dynamic_def.mi_struct_bytes         = any_array ? mi_bytes_array : mi_bytes_simple;
    dynamic_def.name                    = any_array ? "StandardTextureArray_v1" : "Standard_v1";

    MaterialVariantKey route_key = input_key;
    route_key.surface_type        = SurfaceType::Standard;
    route_key.texture_source_mode = any_array ? TextureSourceMode::Array : TextureSourceMode::Simple;
    route_key.texture_source_bits = 0;
    route_key.feature_bits        = any_array
        ? (VF_HasBaseColorTex | VF_HasNormalTex)
        : (VF_HasBaseColorTex | VF_HasNormalTex | VF_HasRoughnessTex);

    MaterialVariantKey assemble_key = route_key;
    if (has_per_slot_mode)
    {
        assemble_key.SetTextureSourceMode(TextureSlot::BaseColor, resolved_base);
        assemble_key.SetTextureSourceMode(TextureSlot::Normal,    resolved_normal);
        assemble_key.SetTextureSourceMode(TextureSlot::Roughness, resolved_rough);
    }
    else
    {
        assemble_key.SetTextureSourceMode(TextureSlot::BaseColor, route_key.texture_source_mode);
        assemble_key.SetTextureSourceMode(TextureSlot::Normal,    route_key.texture_source_mode);
        assemble_key.SetTextureSourceMode(TextureSlot::Roughness, route_key.texture_source_mode);
    }

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(route_key);
    if (!var_desc)
    {
        std::fprintf(stderr,
            "[Standard] VariantRegistry lookup failed (route_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X feature_bits=0x%08X any_array=%d)\n",
            static_cast<unsigned long long>(route_key.Hash()),
            static_cast<unsigned>(route_key.surface_type),
            static_cast<unsigned>(route_key.geometry_mode),
            static_cast<unsigned>(route_key.texture_source_mode),
            route_key.texture_source_bits,
            route_key.feature_bits,
            any_array ? 1 : 0);
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(assemble_key, PlatformBackend::PC, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        result.vertex_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[Standard] CompileCompositorMaterial failed\n");
    return mci;
}

// Unified factory — TextureSourceMode::Simple  -> sampler2D  (classic single-texture Standard)
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

// Compat wrappers — keep the two named entry-points so MaterialLibrary.cpp
// does not need to change its dispatch table.
MaterialCreateInfo *CreateStandard(const contract::PhysicalDeviceProfileLite *profile,
                                   const Material3DCreateConfig *cfg)
{
    return CreateStandard(profile, cfg, TextureSourceMode::Simple);
}

MaterialCreateInfo *CreateStandardTextureArray(const contract::PhysicalDeviceProfileLite *profile,
                                               const Material3DCreateConfig *cfg)
{
    return CreateStandard(profile, cfg, TextureSourceMode::Array);
}

}//namespace hgl::graph::mtl
