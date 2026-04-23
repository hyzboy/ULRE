#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/Sprite2DMaterialCreateConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<cstdio>
#include<vector>
#include<hgl/mtl/MaterialVariantRegistry.h>
#include"Build3DCommon.h"

namespace hgl::graph::mtl{
namespace
{
    // vec2 Position + vec2 TexCoord — unit square mesh, center at (0,0), size 1x1
    constexpr FixedVertexEntry SPRITE2D_VERTEX[] = {
        { VAT_VEC2, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
    };

    const UBOSemanticSet SPRITE2D_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };

    const SSBOSemanticSet SPRITE2D_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
    };

    constexpr SamplerSlot SPRITE2D_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
    };
    constexpr uint32_t SPRITE2D_TEX_SLOT_COUNT = uint32_t(sizeof(SPRITE2D_TEX_SLOTS) / sizeof(SPRITE2D_TEX_SLOTS[0]));

    const StaticMaterialDef SPRITE2D_DEF_TEMPLATE {
        "Sprite2D",
        PrimitiveType::Triangles,
        SPRITE2D_VERTEX,
        uint32_t(sizeof(SPRITE2D_VERTEX) / sizeof(SPRITE2D_VERTEX[0])),
        &SPRITE2D_BASE_UBOS,
        &SPRITE2D_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::Sprite2DTransform,
    };

    static MaterialCreateInfo* CreateSprite2DInternal(
        const contract::PhysicalDeviceProfileLite* profile,
        Sprite2DMaterialCreateConfig* cfg,
        GeometryMode geo_mode,
        const char* vs_path,
        const char* log_tag)
    {
        if (!cfg)
            return nullptr;

        cfg->local_to_world  = true;
        cfg->material_instance = true;

        const bool use_array = cfg->use_texture_array;

        StaticTextureSamplerDescriptors dynamic_samplers;
        for (uint32_t i = 0; i < SPRITE2D_TEX_SLOT_COUNT; ++i)
            AddTextureSampler(dynamic_samplers, SPRITE2D_TEX_SLOTS[i],
                              use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D,
                              0, 0,
                              cfg->base_color_channel);

        SSBOSemanticSet dynamic_ssbos = SPRITE2D_BASE_SSBOS;
        if (use_array)
            AddSSBODescriptor(dynamic_ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

        StaticMaterialDef dynamic_def   = SPRITE2D_DEF_TEMPLATE;
        dynamic_def.texture_samplers    = &dynamic_samplers;
        dynamic_def.ssbo_descriptors    = &dynamic_ssbos;

        MaterialVariantKey lookup_key   = build3d::MakeSprite2DKeyBase(cfg->blend_mode);
        lookup_key.geometry_mode        = geo_mode;

        MaterialVariantKey assemble_key = lookup_key;
        if (use_array)
            assemble_key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Array);

        std::fprintf(stderr, "[%s] use_array=%d  blend=%d  samplerType=%s\n",
            log_tag, (int)use_array, (int)cfg->blend_mode,
            use_array ? "Sampler2DArray" : "Sampler2D");

        MaterialVariantKey resolved_lookup_key{};
        const MaterialVariantDesc* var_desc =
            GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(lookup_key, &resolved_lookup_key);
        if (!var_desc)
        {
            std::fprintf(stderr, "[%s] VariantRegistry lookup failed\n", log_tag);
            return nullptr;
        }

        std::fprintf(stderr,
            "[%s] variant found: %s, lookup_hash=%llu resolved_lookup_hash=%llu assemble_hash=%llu\n",
            log_tag,
            var_desc->variant_name.c_str(),
            static_cast<unsigned long long>(lookup_key.Hash()),
            static_cast<unsigned long long>(resolved_lookup_key.Hash()),
            static_cast<unsigned long long>(assemble_key.Hash()));

        CompositorAssembler assembler;
        auto result = assembler.Assemble(assemble_key, *var_desc);
        if (!result.success)
        {
            std::fprintf(stderr, "[%s] CompositorAssembler failed: %s\n",
                log_tag, result.error_message.c_str());
            return nullptr;
        }

        std::fprintf(stderr, "[%s] assemble OK, compiling material...\n", log_tag);

        MaterialCreateInfo* mci = CompileCompositorMaterial(
            profile,
            dynamic_def,
            result.vertex_glsl,
            result.fragment_glsl,
            cfg);

        if (!mci)
            std::fprintf(stderr, "[%s] CompileCompositorMaterial failed\n", log_tag);
        else
            std::fprintf(stderr, "[%s] material created OK\n", log_tag);
        return mci;
    }
}//namespace

// ── Camera-facing (world-space size) ─────────────────────────────────────────

MaterialCreateInfo* CreateSprite2DCameraFacing(const contract::PhysicalDeviceProfileLite* profile,
                                               Sprite2DMaterialCreateConfig* cfg)
{
    return CreateSprite2DInternal(profile, cfg,
        GeometryMode::Sprite2DCameraFacing,
        "compositor/main_forward_sprite2d_dynamic.vert.glsl",
        "Sprite2DCameraFacing");
}

static MaterialCreateInfo* Sprite2DCameraFacing_Adapter(
    const contract::PhysicalDeviceProfileLite* profile,
    const MaterialVariantKey&,
    MaterialCreateConfig* cfg)
{ return CreateSprite2DCameraFacing(profile, static_cast<Sprite2DMaterialCreateConfig*>(cfg)); }

// ── Axis-locked (fixed pixel size) ───────────────────────────────────────────

MaterialCreateInfo* CreateSprite2DAxisLocked(const contract::PhysicalDeviceProfileLite* profile,
                                             Sprite2DMaterialCreateConfig* cfg)
{
    if (cfg) cfg->axis_locked = true;
    return CreateSprite2DInternal(profile, cfg,
        GeometryMode::Sprite2DAxisLocked,
        "compositor/main_forward_sprite2d_fixed.vert.glsl",
        "Sprite2DAxisLocked");
}

static MaterialCreateInfo* Sprite2DAxisLocked_Adapter(
    const contract::PhysicalDeviceProfileLite* profile,
    const MaterialVariantKey&,
    MaterialCreateConfig* cfg)
{ return CreateSprite2DAxisLocked(profile, static_cast<Sprite2DMaterialCreateConfig*>(cfg)); }

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Sprite2DCameraFacing, "Sprite2DCameraFacing", hgl::graph::mtl::Sprite2DCameraFacing_Adapter)
ULRE_REGISTER_PRESET_FACTORY(Sprite2DAxisLocked,   "Sprite2DAxisLocked",   hgl::graph::mtl::Sprite2DAxisLocked_Adapter)
