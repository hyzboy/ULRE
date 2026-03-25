#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<cstdio>
#include<unordered_map>

namespace hgl::graph::mtl{

bool ValidateBuiltinMaterialVariants(const std::string &shader_library_path,
                                     std::vector<std::string> &diagnostics)
{
    return GetBuiltinVariantRegistry().ValidateBuiltinVariantTemplates(shader_library_path,diagnostics);
}

MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id)
{
    MaterialVariantKey key{};

    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            key.SetVertexAttribEnabled(VertexAttrib::Color);
            break;
        case MaterialPreset::PureColor2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            break;
        case MaterialPreset::PureTexture2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            key.texture_source_mode = TextureSourceMode::Simple;
            key.SetHasTexture(SamplerSlot::BaseColor);
            break;
        case MaterialPreset::RectTexture2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::ScreenRect;
            key.texture_source_mode = TextureSourceMode::Simple;
            key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
            break;
        case MaterialPreset::Text2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            key.texture_source_mode = TextureSourceMode::Atlas;
            key.SetHasTexture(SamplerSlot::BaseColor);
            break;

        case MaterialPreset::PureColor3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            break;
        case MaterialPreset::VertexColor3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.SetVertexAttribEnabled(VertexAttrib::Color);
            break;
        case MaterialPreset::VertexLuminance3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.SetVertexAttribEnabled(VertexAttrib::Luminance);
            break;
        case MaterialPreset::VertexLuminance2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.SetVertexAttribEnabled(VertexAttrib::Luminance);
            key.SetVertexAttribEnabled(VertexAttrib::Position);
            break;
        case MaterialPreset::VertexPattleColor3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.SetVertexAttribEnabled(VertexAttrib::Color);
            key.SetDebugShading(true);
            break;
        case MaterialPreset::Gizmo3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.SetDebugShading(true);
            break;
        case MaterialPreset::TerrainGrid:
            key.surface_type = SurfaceType::Terrain;
            key.geometry_mode = GeometryMode::Mesh3D;
            break;
        case MaterialPreset::SkyMinimal:
            key.surface_type = SurfaceType::Sky;
            key.geometry_mode = GeometryMode::Mesh3D;
            break;
        case MaterialPreset::Billboard2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::BillboardCameraFacing;
            key.texture_source_mode = TextureSourceMode::Simple;
            key.SetHasTexture(SamplerSlot::BaseColor);
            break;
        case MaterialPreset::Standard:
            key.surface_type = SurfaceType::Standard;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.texture_source_mode = TextureSourceMode::Simple;
            key.SetTextureSourceMode(SamplerSlot::BaseColor, TextureSourceMode::Simple);
            key.SetTextureSourceMode(SamplerSlot::Normal,    TextureSourceMode::Simple);
            key.SetTextureSourceMode(SamplerSlot::Roughness, TextureSourceMode::Simple);
            break;
        case MaterialPreset::PBRColor3D:
            key.surface_type = SurfaceType::Standard;
            key.geometry_mode = GeometryMode::Mesh3D;
            break;
        default:
            break;
    }

    return key;
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return "VertexColor2D";
        case MaterialPreset::PureColor2D:           return "PureColor2D";
        case MaterialPreset::PureTexture2D:         return "PureTexture2D";
        case MaterialPreset::RectTexture2D:         return "RectTexture2D";
        case MaterialPreset::Text2D:                return "Text2D";
        case MaterialPreset::PureColor3D:           return "PureColor3D";
        case MaterialPreset::VertexColor3D:         return "VertexColor3D";
        case MaterialPreset::VertexLuminance3D:     return "VertexLuminance3D";
        case MaterialPreset::VertexLuminance2D:     return "VertexLuminance2D";
        case MaterialPreset::VertexPattleColor3D:   return "VertexPattleColor3D";
        case MaterialPreset::Gizmo3D:               return "Gizmo3D";
        case MaterialPreset::TerrainGrid:           return "TerrainGrid";
        case MaterialPreset::SkyMinimal:            return "SkyMinimal";
        case MaterialPreset::Billboard2D:           return "Billboard2D";
        case MaterialPreset::Standard:              return "Standard";
        case MaterialPreset::PBRColor3D:            return "PBRColor3D";
        default:                                    return nullptr;
    }
}

MaterialCreateInfo *CreateStandardVariant(const contract::PhysicalDeviceProfileLite *profile,
                                          const MaterialVariantKey &key,
                                          const Material3DCreateConfig *cfg);
MaterialCreateInfo *CreateRectTextureVariant(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             const Material2DCreateConfig *cfg);

namespace {

using VariantFactory = MaterialCreateInfo*(*)(const contract::PhysicalDeviceProfileLite*, const MaterialVariantKey &, MaterialCreateConfig*);

static MaterialCreateInfo* F_VertexColor2D      (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateVertexColor2D      (p,(const Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_PureColor2D        (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreatePureColor2D        (p,(Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_PureTexture2D      (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreatePureTexture2D      (p,(const Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_RectTexture2D      (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &k, MaterialCreateConfig* c) { return ::hgl::graph::mtl::CreateRectTextureVariant (p,k,(Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_Text2D             (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateText2D             (p,(const Text2DMaterialCreateConfig*)c); }
static MaterialCreateInfo* F_PureColor3D        (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreatePureColor3D        (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexColor3D      (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateVertexColor3D      (p,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexLuminance3D  (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateVertexLuminance3D  (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexLuminance2D  (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateVertexLuminance2D  (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexPattleColor3D(const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateVertexPattleColor3D(p,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_Gizmo3D            (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateGizmo3D            (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_TerrainGrid        (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateTerrainGrid        (p,(const TerrainGridCreateConfig*)c); }
static MaterialCreateInfo* F_SkyMinimal         (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateSkyMinimal         (p,(const SkyMinimalCreateConfig*)c); }
static MaterialCreateInfo* F_Billboard2D        (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreateBillboard2D        (p,(BillboardMaterialCreateConfig*)c); }
static MaterialCreateInfo* F_Standard           (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &k, MaterialCreateConfig* c) { return ::hgl::graph::mtl::CreateStandardVariant    (p,k,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_PBRColor3D         (const contract::PhysicalDeviceProfileLite* p, const MaterialVariantKey &, MaterialCreateConfig* c) { return CreatePBRColor3D         (p,(PBRColor3DMaterialCreateConfig*)c); }

static std::unordered_map<uint64, VariantFactory> BuildFactoryMap()
{
    std::unordered_map<uint64, VariantFactory> m;
    m.reserve(20);
    auto reg = [&](MaterialPreset p, VariantFactory f) {
        m[MapPresetToVariantKey(p).Hash()] = f;
    };
    reg(MaterialPreset::VertexColor2D,        F_VertexColor2D);
    reg(MaterialPreset::PureColor2D,          F_PureColor2D);
    reg(MaterialPreset::PureTexture2D,        F_PureTexture2D);
    reg(MaterialPreset::RectTexture2D,        F_RectTexture2D);
    reg(MaterialPreset::Text2D,               F_Text2D);
    reg(MaterialPreset::PureColor3D,          F_PureColor3D);
    reg(MaterialPreset::VertexColor3D,        F_VertexColor3D);
    reg(MaterialPreset::VertexLuminance3D,    F_VertexLuminance3D);
    reg(MaterialPreset::VertexLuminance2D,    F_VertexLuminance2D);
    reg(MaterialPreset::VertexPattleColor3D,  F_VertexPattleColor3D);
    reg(MaterialPreset::Gizmo3D,              F_Gizmo3D);
    reg(MaterialPreset::TerrainGrid,          F_TerrainGrid);
    reg(MaterialPreset::SkyMinimal,           F_SkyMinimal);
    reg(MaterialPreset::Billboard2D,          F_Billboard2D);
    reg(MaterialPreset::Standard,             F_Standard);
    reg(MaterialPreset::PBRColor3D,           F_PBRColor3D);

    // Register canonical Array variant keys (texture_source_bits = 0, sampler_feature_bits = 0)
    {
        MaterialVariantKey k;
        k.surface_type = SurfaceType::Standard;
        k.geometry_mode = GeometryMode::Mesh3D;
        k.texture_source_mode = TextureSourceMode::Array;
        m[k.Hash()] = F_Standard;
    }
    {
        MaterialVariantKey k;
        k.surface_type = SurfaceType::Unlit;
        k.geometry_mode = GeometryMode::ScreenRect;
        k.texture_source_mode = TextureSourceMode::Array;
        m[k.Hash()] = F_RectTexture2D;
    }
    return m;
}

static const std::unordered_map<uint64, VariantFactory>& GetFactoryMap()
{
    static const auto s_map = BuildFactoryMap();
    return s_map;
}

} // anonymous namespace

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             MaterialCreateConfig *cfg)
{
    static const bool s_startup_variant_validation_done = []()
    {
        std::vector<std::string> diagnostics;

        const bool ok = ValidateBuiltinMaterialVariants("ShaderLibrary",diagnostics);
        if(ok)
        {
            std::fprintf(stderr,"[MaterialLibrary] Startup variant validation passed.\n");
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

    const auto &m = GetFactoryMap();

    auto it = m.find(key.Hash());
    if(it == m.end())
    {
        // Canonicalize: strip per-slot texture_source_bits and sampler_feature_bits, retry
        MaterialVariantKey canon = key;
        canon.texture_source_bits = 0;
        canon.sampler_feature_bits = 0;
        it = m.find(canon.Hash());
    }
    if(it != m.end())
        return it->second(profile, key, cfg);

    std::fprintf(stderr,
        "[MaterialLibrary] CreateMaterialCreateInfo failed: no factory for variant key (key_hash=%llu surface=%u geom=%u tex_mode=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X)\n",
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

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    return CreateMaterialCreateInfo(profile, MapPresetToVariantKey(mtl_id), cfg);
}

}//namespace hgl::graph::mtl
