#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/shadergen/ShaderGenPathConfig.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<cstdio>

namespace hgl::graph::mtl{

bool ValidateBuiltinMaterialVariants(const std::string &shader_library_path,
                                     std::vector<std::string> &diagnostics)
{
    return GetBuiltinVariantRegistry().ValidateBuiltinVariantTemplates(shader_library_path,diagnostics);
}

std::string GetBuiltinMaterialVariantSnapshot()
{
    return GetBuiltinVariantRegistry().DumpSnapshot();
}

namespace {

using MakeVariantKeyProc = MaterialVariantKey (*)();

struct MaterialPresetMeta
{
    MaterialPreset preset;
    const char *name;
    MakeVariantKeyProc make_key;
};

static MaterialVariantKey MakeVertexColor2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Quad2D;
    key.SetVertexAttribEnabled(VertexAttrib::Color);
    return key;
}

static MaterialVariantKey MakePureColor2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Quad2D;
    return key;
}

static MaterialVariantKey MakePureTexture2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Quad2D;
    key.texture_source_mode = TextureSourceMode::Simple;
    key.SetHasTexture(SamplerSlot::BaseColor);
    return key;
}

static MaterialVariantKey MakeText2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Quad2D;
    key.texture_source_mode = TextureSourceMode::Atlas;
    key.SetHasTexture(SamplerSlot::BaseColor);
    return key;
}

static MaterialVariantKey MakePureColor3DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    return key;
}

static MaterialVariantKey MakeVertexColor3DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.SetVertexAttribEnabled(VertexAttrib::Color);
    return key;
}

static MaterialVariantKey MakeVertexLuminance3DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.SetVertexAttribEnabled(VertexAttrib::Luminance);
    return key;
}

static MaterialVariantKey MakeVertexLuminance2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.SetVertexAttribEnabled(VertexAttrib::Luminance);
    key.SetVertexAttribEnabled(VertexAttrib::Position);
    return key;
}

static MaterialVariantKey MakeVertexPattleColor3DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.SetVertexAttribEnabled(VertexAttrib::Color);
    key.SetDebugShading(true);
    return key;
}

static MaterialVariantKey MakeGizmo3DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.SetDebugShading(true);
    return key;
}

static MaterialVariantKey MakeTerrainGridKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Terrain;
    key.geometry_mode = GeometryMode::Mesh3D;
    return key;
}

static MaterialVariantKey MakeSkyMinimalKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Sky;
    key.geometry_mode = GeometryMode::Mesh3D;
    return key;
}

static MaterialVariantKey MakeBillboard2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::BillboardCameraFacing;
    key.texture_source_mode = TextureSourceMode::Simple;
    key.SetHasTexture(SamplerSlot::BaseColor);
    key.blend_mode = BlendMode::Transparent;
    key.pass_hint = PassType::ForwardTransparent;
    return key;
}

static MaterialVariantKey MakeStandardKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Standard;
    key.geometry_mode = GeometryMode::Mesh3D;
    key.texture_source_mode = TextureSourceMode::Simple;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    key.SetTextureSourceMode(SamplerSlot::Normal,    TextureSourceMode::Simple);
    return key;
}

static MaterialVariantKey MakePBRColor3DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Standard;
    key.geometry_mode = GeometryMode::Mesh3D;
    return key;
}

static const MaterialPresetMeta kPresetMetaList[] =
{
    {MaterialPreset::VertexColor2D,       "VertexColor2D",       MakeVertexColor2DKey},
    {MaterialPreset::PureColor2D,         "PureColor2D",         MakePureColor2DKey},
    {MaterialPreset::PureTexture2D,       "PureTexture2D",       MakePureTexture2DKey},
    {MaterialPreset::Text2D,              "Text2D",              MakeText2DKey},
    {MaterialPreset::PureColor3D,         "PureColor3D",         MakePureColor3DKey},
    {MaterialPreset::VertexColor3D,       "VertexColor3D",       MakeVertexColor3DKey},
    {MaterialPreset::VertexLuminance3D,   "VertexLuminance3D",   MakeVertexLuminance3DKey},
    {MaterialPreset::VertexLuminance2D,   "VertexLuminance2D",   MakeVertexLuminance2DKey},
    {MaterialPreset::VertexPattleColor3D, "VertexPattleColor3D", MakeVertexPattleColor3DKey},
    {MaterialPreset::Gizmo3D,             "Gizmo3D",             MakeGizmo3DKey},
    {MaterialPreset::TerrainGrid,         "TerrainGrid",         MakeTerrainGridKey},
    {MaterialPreset::SkyMinimal,          "SkyMinimal",          MakeSkyMinimalKey},
    {MaterialPreset::Billboard2D,         "Billboard2D",         MakeBillboard2DKey},
    {MaterialPreset::Standard,            "Standard",            MakeStandardKey},
    {MaterialPreset::PBRColor3D,          "PBRColor3D",          MakePBRColor3DKey},
};

static const MaterialPresetMeta *FindMaterialPresetMeta(const MaterialPreset preset)
{
    for(const auto &meta:kPresetMetaList)
        if(meta.preset==preset)
            return &meta;

    return nullptr;
}

}

MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id)
{
    const MaterialPresetMeta *meta=FindMaterialPresetMeta(mtl_id);
    if(!meta||!meta->make_key)
        return MaterialVariantKey{};

    return meta->make_key();
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    const MaterialPresetMeta *meta=FindMaterialPresetMeta(mtl_id);
    return meta?meta->name:nullptr;
}

MaterialCreateInfo *CreateStandardVariant(const contract::PhysicalDeviceProfileLite *profile,
                                          const MaterialVariantKey &key,
                                          const Material3DCreateConfig *cfg);
MaterialCreateInfo *CreatePureTextureVariant(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             const Material2DCreateConfig *cfg);

namespace {
static MaterialCreateInfo *DispatchVariantFactory(
    const MaterialPreset factory_type,
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    switch(factory_type)
    {
        case MaterialPreset::VertexColor2D:        return CreateVertexColor2D(profile,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::PureColor2D:          return CreatePureColor2D(profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::PureTexture2D:        return CreatePureTextureVariant(profile,key,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::Text2D:               return CreateText2D(profile,(const Text2DMaterialCreateConfig *)cfg);

        case MaterialPreset::PureColor3D:          return CreatePureColor3D(profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexColor3D:        return CreateVertexColor3D(profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexLuminance3D:    return CreateVertexLuminance3D(profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexLuminance2D:    return CreateVertexLuminance2D(profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexPattleColor3D:  return CreateVertexPattleColor3D(profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::Gizmo3D:              return CreateGizmo3D(profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::TerrainGrid:          return CreateTerrainGrid(profile,(const TerrainGridCreateConfig *)cfg);
        case MaterialPreset::SkyMinimal:           return CreateSkyMinimal(profile,(const SkyMinimalCreateConfig *)cfg);
        case MaterialPreset::Billboard2D:          return CreateBillboard2D(profile,(BillboardMaterialCreateConfig *)cfg);
        case MaterialPreset::Standard:             return CreateStandardVariant(profile,key,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::PBRColor3D:           return CreatePBRColor3D(profile,(PBRColor3DMaterialCreateConfig *)cfg);

        default:
            return nullptr;
    }
}

} // anonymous namespace

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             MaterialCreateConfig *cfg)
{
    static const bool s_startup_variant_validation_done = []()
    {
        std::vector<std::string> diagnostics;

        const bool ok = ValidateBuiltinMaterialVariants(GetShaderLibraryPath(),diagnostics);
        if(ok)
        {
            std::printf("[MaterialLibrary] Startup variant validation passed.\n");
            return true;
        }

        std::fprintf(stderr,
                     "[MaterialLibrary] Startup variant validation failed: %zu issue(s).\n",
                     diagnostics.size());

        for(const auto &msg:diagnostics)
            std::fprintf(stderr,"[MaterialLibrary] %s\n",msg.c_str());

        return false;
    }();

    (void)s_startup_variant_validation_done;

    if(!cfg)
    {
        std::fprintf(stderr, "[MaterialLibrary] CreateMaterialCreateInfo failed: cfg is null\n");
        return nullptr;
    }

    if(!profile)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo warning: profile is null (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.texture_source_mode),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
    }

    MaterialVariantKey resolved_key{};
    const MaterialVariantDesc *variant_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(key,&resolved_key);
    if(!variant_desc)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: no registered variant (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.texture_source_mode),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits);
        return nullptr;
    }

    if(!variant_desc->has_factory_type)
    {
        std::fprintf(stderr,
            "[MaterialLibrary] CreateMaterialCreateInfo failed: variant has no factory_type assigned (variant=%s key_hash=%llu)\n",
            variant_desc->variant_name.c_str(),
            static_cast<unsigned long long>(key.Hash()));
        return nullptr;
    }

    if(MaterialCreateInfo *mci=DispatchVariantFactory(variant_desc->factory_type,profile,key,cfg))
        return mci;

    std::fprintf(stderr,
        "[MaterialLibrary] CreateMaterialCreateInfo failed: factory dispatch failed (variant=%s factory_type=%u key_hash=%llu resolved_key_hash=%llu)\n",
        variant_desc->variant_name.c_str(),
        static_cast<unsigned>(variant_desc->factory_type),
        static_cast<unsigned long long>(key.Hash()),
        static_cast<unsigned long long>(resolved_key.Hash()));
    return nullptr;
}

void ApplyCreateConfigToVariantKey(MaterialVariantKey &key, const MaterialCreateConfig *cfg)
{
    if (!cfg)
        return;

    if (cfg->override_geometry_mode)
        key.geometry_mode = cfg->geometry_mode_override;

    if (cfg->texture_source_bits_override != 0)
    {
        key.texture_source_bits = cfg->texture_source_bits_override;

        // Derive key.texture_source_mode from per-slot bits (first non-None mode wins).
        key.texture_source_mode = TextureSourceMode::None;
        for (uint8_t s = 0; s < uint8_t(SamplerSlot::RANGE_SIZE); ++s)
        {
            const TextureSourceMode m = TextureSourceMode((key.texture_source_bits >> (uint32_t(s) * MaterialVariantKey::TextureSourceBitsPerSlot))
                                      & MaterialVariantKey::TextureSourceMask);
            if (m != TextureSourceMode::None) { key.texture_source_mode = m; break; }
        }

        // If caller did not provide an explicit sampler feature override,
        // derive mask from per-slot texture source bits to keep key coherent.
        if (cfg->sampler_feature_bits_override != 0)
            key.sampler_feature_bits = cfg->sampler_feature_bits_override;
        else
        {
            key.sampler_feature_bits = 0;
            for (uint8_t s = 0; s < uint8_t(SamplerSlot::RANGE_SIZE); ++s)
            {
                const TextureSourceMode mode = TextureSourceMode((key.texture_source_bits >> (uint32_t(s) * MaterialVariantKey::TextureSourceBitsPerSlot))
                                          & MaterialVariantKey::TextureSourceMask);
                if (mode != TextureSourceMode::None)
                    key.sampler_feature_bits |= SamplerFeatureBit(SamplerSlot(s));
            }
        }
    }
    else if (cfg->sampler_feature_bits_override != 0)
    {
        key.sampler_feature_bits = cfg->sampler_feature_bits_override;
    }
}

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    MaterialVariantKey key = MapPresetToVariantKey(mtl_id);

    ApplyCreateConfigToVariantKey(key, cfg);

    return CreateMaterialCreateInfo(profile, key, cfg);
}

}//namespace hgl::graph::mtl
