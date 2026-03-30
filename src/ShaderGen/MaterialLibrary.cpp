#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/shadergen/ShaderLibraryPath.h>
#include<hgl/shadergen/device/DeviceProfile.h>
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

struct PresetResolveEntry
{
    MaterialPreset preset;
    const char *name;
    MaterialPreset canonical_preset;
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
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    key.SetHasTexture(SamplerSlot::BaseColor);
    return key;
}

static MaterialVariantKey MakeText2DKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::Quad2D;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Atlas);
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

static MaterialVariantKey MakeBillboard2DDynamicKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::BillboardCameraFacing;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
    key.SetHasTexture(SamplerSlot::BaseColor);
    key.blend_mode = BlendMode::Transparent;
    key.pass_hint = PassType::ForwardTransparent;
    return key;
}

static MaterialVariantKey MakeBillboard2DFixedKey()
{
    MaterialVariantKey key{};
    key.surface_type = SurfaceType::Unlit;
    key.geometry_mode = GeometryMode::BillboardAxisLocked;
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
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
    key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
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

static bool IsSemanticMaterialPreset(const MaterialPreset preset)
{
    switch(preset)
    {
        case MaterialPreset::HumanSkin:
        case MaterialPreset::AmphibiansSkin:
        case MaterialPreset::Wood:
        case MaterialPreset::TreeBark:
        case MaterialPreset::Stone:
        case MaterialPreset::Leaf:
        case MaterialPreset::Metal:
        case MaterialPreset::BirdFeathers:
        case MaterialPreset::Scales:
            return true;
        default:
            return false;
    }
}

static const PresetResolveEntry kPresetResolveTable[] =
{
    {MaterialPreset::VertexColor2D,       "VertexColor2D",       MaterialPreset::VertexColor2D,       MakeVertexColor2DKey},
    {MaterialPreset::PureColor2D,         "PureColor2D",         MaterialPreset::PureColor2D,         MakePureColor2DKey},
    {MaterialPreset::PureTexture2D,       "PureTexture2D",       MaterialPreset::PureTexture2D,       MakePureTexture2DKey},
    {MaterialPreset::Text2D,              "Text2D",              MaterialPreset::Text2D,              MakeText2DKey},
    {MaterialPreset::PureColor3D,         "PureColor3D",         MaterialPreset::PureColor3D,         MakePureColor3DKey},
    {MaterialPreset::VertexColor3D,       "VertexColor3D",       MaterialPreset::VertexColor3D,       MakeVertexColor3DKey},
    {MaterialPreset::VertexLuminance3D,   "VertexLuminance3D",   MaterialPreset::VertexLuminance3D,   MakeVertexLuminance3DKey},
    {MaterialPreset::VertexLuminance2D,   "VertexLuminance2D",   MaterialPreset::VertexLuminance2D,   MakeVertexLuminance2DKey},
    {MaterialPreset::VertexPattleColor3D, "VertexPattleColor3D", MaterialPreset::VertexPattleColor3D, MakeVertexPattleColor3DKey},
    {MaterialPreset::Gizmo3D,             "Gizmo3D",             MaterialPreset::Gizmo3D,             MakeGizmo3DKey},
    {MaterialPreset::TerrainGrid,         "TerrainGrid",         MaterialPreset::TerrainGrid,         MakeTerrainGridKey},
    {MaterialPreset::SkyMinimal,          "SkyMinimal",          MaterialPreset::SkyMinimal,          MakeSkyMinimalKey},
    {MaterialPreset::Billboard2DDynamic,  "Billboard2DDynamic",  MaterialPreset::Billboard2DDynamic,  MakeBillboard2DDynamicKey},
    {MaterialPreset::Billboard2DFixed,    "Billboard2DFixed",    MaterialPreset::Billboard2DFixed,    MakeBillboard2DFixedKey},
    {MaterialPreset::Standard,            "Standard",            MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::PBRColor3D,          "PBRColor3D",          MaterialPreset::PBRColor3D,          MakePBRColor3DKey},
    // Semantic aliases (LOD reserved, current lod=0 target is Standard)
    {MaterialPreset::HumanSkin,           "HumanSkin",           MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::AmphibiansSkin,      "AmphibiansSkin",      MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::Wood,                "Wood",                MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::TreeBark,            "TreeBark",            MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::Stone,               "Stone",               MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::Leaf,                "Leaf",                MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::Metal,               "Metal",               MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::BirdFeathers,        "BirdFeathers",        MaterialPreset::Standard,            MakeStandardKey},
    {MaterialPreset::Scales,              "Scales",              MaterialPreset::Standard,            MakeStandardKey},
};

static const PresetResolveEntry *FindPresetResolveEntry(const MaterialPreset preset)
{
    for(const auto &entry:kPresetResolveTable)
        if(entry.preset==preset)
            return &entry;

    return nullptr;
}

static bool ValidatePresetResolveTable()
{
    bool ok=true;

    for(size_t i=0;i<sizeof(kPresetResolveTable)/sizeof(kPresetResolveTable[0]);++i)
    {
        const auto &entry=kPresetResolveTable[i];

        // Check duplicate preset entries.
        for(size_t j=i+1;j<sizeof(kPresetResolveTable)/sizeof(kPresetResolveTable[0]);++j)
        {
            if(entry.preset==kPresetResolveTable[j].preset)
            {
                std::fprintf(stderr,
                    "[MaterialLibrary] PresetResolveTable duplicate preset=%u\n",
                    static_cast<unsigned>(entry.preset));
                ok=false;
            }
        }

        if(!entry.make_key)
        {
            std::fprintf(stderr,
                "[MaterialLibrary] PresetResolveTable missing make_key preset=%u (%s)\n",
                static_cast<unsigned>(entry.preset),
                entry.name?entry.name:"<null>");
            ok=false;
            continue;
        }

        // Canonical preset must be resolvable in current table.
        const PresetResolveEntry *canonical=FindPresetResolveEntry(entry.canonical_preset);
        if(!canonical)
        {
            std::fprintf(stderr,
                "[MaterialLibrary] PresetResolveTable missing canonical preset=%u for preset=%u\n",
                static_cast<unsigned>(entry.canonical_preset),
                static_cast<unsigned>(entry.preset));
            ok=false;
            continue;
        }

        const MaterialVariantKey route_key=entry.make_key();
        const MaterialVariantKey canonical_key=canonical->make_key();
        if(route_key.Hash()!=canonical_key.Hash())
        {
            std::fprintf(stderr,
                "[MaterialLibrary] PresetResolveTable route mismatch preset=%u route=%llu canonical=%llu\n",
                static_cast<unsigned>(entry.preset),
                static_cast<unsigned long long>(route_key.Hash()),
                static_cast<unsigned long long>(canonical_key.Hash()));
            ok=false;
        }
    }

    return ok;
}

}

MaterialLOD GetDefaultMaterialLOD()
{
    // Temporary bootstrap fallback: current runtime only exposes one built-in material
    // implementation level. Future forward / VBuffer paths may choose LOD from richer context
    // instead of using a single global default.
    return MaterialLOD::Base;
}

MaterialPreset ResolveMaterialPresetForLOD(const MaterialPreset preset,
                                           const MaterialLOD lod)
{
    switch(lod)
    {
        case MaterialLOD::Base:
        default:
            // Current bootstrap behavior: semantic presets still reuse the Standard family.
            if(IsSemanticMaterialPreset(preset))
                return MaterialPreset::Standard;

            return preset;
    }
}

MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id)
{
    static const bool s_preset_resolve_table_ok=[]()
    {
        const bool ok=ValidatePresetResolveTable();
        if(ok)
            std::printf("[MaterialLibrary] PresetResolveTable validation passed.\n");
        else
            std::fprintf(stderr,"[MaterialLibrary] PresetResolveTable validation failed.\n");
        return ok;
    }();

    (void)s_preset_resolve_table_ok;

    const MaterialPreset resolved_preset = ResolveMaterialPresetForLOD(mtl_id, GetDefaultMaterialLOD());

    // Commit B: table-first mapping path.
    const PresetResolveEntry *entry=FindPresetResolveEntry(resolved_preset);
    if(entry&&entry->make_key)
    {
        return entry->make_key();
    }

    // Commit C: unknown preset defense only.
    std::fprintf(stderr,
        "[MaterialLibrary] ERROR: MapPresetToVariantKey unknown preset=%u\n",
        static_cast<unsigned>(mtl_id));
    return MaterialVariantKey{};
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    const PresetResolveEntry *entry=FindPresetResolveEntry(mtl_id);
    return entry?entry->name:nullptr;
}

MaterialCreateInfo *CreateStandardVariant(const contract::PhysicalDeviceProfileLite *profile,
                                          const MaterialVariantKey &key,
                                          const Material3DCreateConfig *cfg);
MaterialCreateInfo *CreatePureTextureVariant(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             const Material2DCreateConfig *cfg);
// Forward declarations for 2D factory functions (refactored to accept MaterialVariantKey)
MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,
                                        const Material2DCreateConfig *cfg,
                                        const MaterialVariantKey &key);
MaterialCreateInfo *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,
                                      Material2DCreateConfig *cfg,
                                      const MaterialVariantKey &key);
MaterialCreateInfo *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,
                                        const Material2DCreateConfig *cfg,
                                        MaterialVariantKey key);
MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,
                                 const Text2DMaterialCreateConfig *cfg,
                                 const MaterialVariantKey &key);

namespace {

using DispatchVariantFactoryProc = MaterialCreateInfo *(*)(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg);

struct VariantFactoryDispatchEntry
{
    MaterialPreset factory_type;
    const char *name;
    DispatchVariantFactoryProc dispatch;
};

static MaterialCreateInfo *DispatchVertexColor2D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    return CreateVertexColor2D(profile,(const Material2DCreateConfig *)cfg,key);
}

static MaterialCreateInfo *DispatchPureColor2D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    return CreatePureColor2D(profile,(Material2DCreateConfig *)cfg,key);
}

static MaterialCreateInfo *DispatchPureTexture2D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    return CreatePureTexture2D(profile,(const Material2DCreateConfig *)cfg,key);
}

static MaterialCreateInfo *DispatchText2D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    return CreateText2D(profile,(const Text2DMaterialCreateConfig *)cfg,key);
}

static MaterialCreateInfo *DispatchPureColor3D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreatePureColor3D(profile,(Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchVertexColor3D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateVertexColor3D(profile,(const Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchVertexLuminance3D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateVertexLuminance3D(profile,(Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchVertexLuminance2D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateVertexLuminance2D(profile,(Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchVertexPattleColor3D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateVertexPattleColor3D(profile,(const Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchGizmo3D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateGizmo3D(profile,(Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchTerrainGrid(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateTerrainGrid(profile,(const TerrainGridCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchSkyMinimal(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateSkyMinimal(profile,(const SkyMinimalCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchBillboard2DDynamic(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateBillboard2DDynamic(profile,(BillboardMaterialCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchBillboard2DFixed(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreateBillboard2DFixed(profile,(BillboardMaterialCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchStandard(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    return CreateStandardVariant(profile,key,(const Material3DCreateConfig *)cfg);
}

static MaterialCreateInfo *DispatchPBRColor3D(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{
    return CreatePBRColor3D(profile,(PBRColor3DMaterialCreateConfig *)cfg);
}

static const VariantFactoryDispatchEntry kVariantFactoryDispatchTable[] =
{
    {MaterialPreset::VertexColor2D,       "VertexColor2D",       DispatchVertexColor2D},
    {MaterialPreset::PureColor2D,         "PureColor2D",         DispatchPureColor2D},
    {MaterialPreset::PureTexture2D,       "PureTexture2D",       DispatchPureTexture2D},
    {MaterialPreset::Text2D,              "Text2D",              DispatchText2D},
    {MaterialPreset::PureColor3D,         "PureColor3D",         DispatchPureColor3D},
    {MaterialPreset::VertexColor3D,       "VertexColor3D",       DispatchVertexColor3D},
    {MaterialPreset::VertexLuminance3D,   "VertexLuminance3D",   DispatchVertexLuminance3D},
    {MaterialPreset::VertexLuminance2D,   "VertexLuminance2D",   DispatchVertexLuminance2D},
    {MaterialPreset::VertexPattleColor3D, "VertexPattleColor3D", DispatchVertexPattleColor3D},
    {MaterialPreset::Gizmo3D,             "Gizmo3D",             DispatchGizmo3D},
    {MaterialPreset::TerrainGrid,         "TerrainGrid",         DispatchTerrainGrid},
    {MaterialPreset::SkyMinimal,          "SkyMinimal",          DispatchSkyMinimal},
    {MaterialPreset::Billboard2DDynamic,  "Billboard2DDynamic",  DispatchBillboard2DDynamic},
    {MaterialPreset::Billboard2DFixed,    "Billboard2DFixed",    DispatchBillboard2DFixed},
    {MaterialPreset::Standard,            "Standard",            DispatchStandard},
    {MaterialPreset::PBRColor3D,          "PBRColor3D",          DispatchPBRColor3D},
};

static const VariantFactoryDispatchEntry *FindVariantFactoryDispatchEntry(const MaterialPreset factory_type)
{
    for(const auto &entry:kVariantFactoryDispatchTable)
        if(entry.factory_type==factory_type)
            return &entry;

    return nullptr;
}

static MaterialCreateInfo *DispatchVariantFactory(
    const MaterialPreset factory_type,
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &key,
    MaterialCreateConfig *cfg)
{
    const VariantFactoryDispatchEntry *entry=FindVariantFactoryDispatchEntry(factory_type);
    return entry&&entry->dispatch?entry->dispatch(profile,key,cfg):nullptr;
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
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
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
            static_cast<unsigned>(key.GetTextureSourceMode(SamplerSlot::BaseColor)),
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

        // Derive primary texture source mode from per-slot bits.
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
    const MaterialLOD lod = GetDefaultMaterialLOD();
    const MaterialPreset resolved_preset = ResolveMaterialPresetForLOD(mtl_id, lod);
    const PresetResolveEntry *entry=FindPresetResolveEntry(mtl_id);
    if(entry&&resolved_preset!=mtl_id)
    {
        std::printf(
            "[MaterialLibrary] Preset alias resolved preset=%u (%s) -> canonical=%u (lod=%u)\n",
            static_cast<unsigned>(mtl_id),
            entry->name?entry->name:"<null>",
            static_cast<unsigned>(resolved_preset),
            static_cast<unsigned>(lod));
    }

    MaterialVariantKey key = MapPresetToVariantKey(resolved_preset);

    ApplyCreateConfigToVariantKey(key, cfg);

    return CreateMaterialCreateInfo(profile, key, cfg);
}

}//namespace hgl::graph::mtl
