#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<unordered_map>

namespace hgl::graph::mtl{

MaterialVariantKey MapPresetToVariantKey(const MaterialPreset mtl_id)
{
    MaterialVariantKey key{};

    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            key.feature_bits = VF_UseVertexColor;
            break;
        case MaterialPreset::PureColor2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            break;
        case MaterialPreset::PureTexture2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            key.texture_source_mode = TextureSourceMode::Tex2D;
            key.feature_bits = VF_HasBaseColorTex;
            break;
        case MaterialPreset::RectTexture2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::ScreenRect;
            key.texture_source_mode = TextureSourceMode::Tex2D;
            key.feature_bits = VF_HasBaseColorTex;
            break;
        case MaterialPreset::RectTexture2DArray:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::ScreenRect;
            key.texture_source_mode = TextureSourceMode::Tex2DArray;
            key.feature_bits = VF_HasBaseColorTex;
            break;
        case MaterialPreset::Text2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Quad2D;
            key.texture_source_mode = TextureSourceMode::Atlas;
            key.feature_bits = VF_HasBaseColorTex;
            break;

        case MaterialPreset::PureColor3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            break;
        case MaterialPreset::VertexColor3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.feature_bits = VF_UseVertexColor;
            break;
        case MaterialPreset::VertexLuminance3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.feature_bits = VF_UseVertexLum;
            break;
        case MaterialPreset::VertexLuminance2D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.feature_bits = VF_UseVertexLum | VF_UsePos2D;
            break;
        case MaterialPreset::VertexPattleColor3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.feature_bits = VF_UseVertexColor | VF_DebugShading;
            break;
        case MaterialPreset::Gizmo3D:
            key.surface_type = SurfaceType::Unlit;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.feature_bits = VF_DebugShading;
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
            key.texture_source_mode = TextureSourceMode::Tex2D;
            key.feature_bits = VF_HasBaseColorTex;
            break;
        case MaterialPreset::Standard:
            key.surface_type = SurfaceType::Standard;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.texture_source_mode = TextureSourceMode::Tex2D;
            key.feature_bits = VF_HasBaseColorTex | VF_HasNormalTex | VF_HasRoughnessTex;
            break;
        case MaterialPreset::StandardTextureArray:
            key.surface_type = SurfaceType::Standard;
            key.geometry_mode = GeometryMode::Mesh3D;
            key.texture_source_mode = TextureSourceMode::Tex2DArray;
            key.feature_bits = VF_HasBaseColorTex | VF_HasNormalTex;
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

bool TryMapVariantKeyToPreset(const MaterialVariantKey &key, MaterialPreset &out_preset)
{
    if (key.surface_type == SurfaceType::Standard && key.geometry_mode == GeometryMode::Mesh3D)
    {
        out_preset = MaterialPreset::Standard;
        return true;
    }

    if (key.surface_type == SurfaceType::Unlit)
    {
        if (key.geometry_mode == GeometryMode::BillboardCameraFacing
         || key.geometry_mode == GeometryMode::BillboardAxisLocked)
        {
            out_preset = MaterialPreset::Billboard2D;
            return true;
        }

        if (key.geometry_mode == GeometryMode::ScreenRect)
        {
            out_preset = (key.texture_source_mode == TextureSourceMode::Tex2DArray)
                ? MaterialPreset::RectTexture2DArray
                : MaterialPreset::RectTexture2D;
            return true;
        }

        if (key.geometry_mode == GeometryMode::Quad2D)
        {
            if ((key.feature_bits & VF_UseVertexColor) != 0)
            {
                out_preset = MaterialPreset::VertexColor2D;
                return true;
            }

            if ((key.feature_bits & VF_HasBaseColorTex) != 0)
            {
                out_preset = MaterialPreset::PureTexture2D;
                return true;
            }

            out_preset = MaterialPreset::PureColor2D;
            return true;
        }

        if (key.geometry_mode == GeometryMode::Mesh3D)
        {
            if ((key.feature_bits & VF_UseVertexLum) != 0)
            {
                out_preset = (key.feature_bits & VF_UsePos2D) != 0
                    ? MaterialPreset::VertexLuminance2D
                    : MaterialPreset::VertexLuminance3D;
                return true;
            }

            if ((key.feature_bits & VF_UseVertexColor) != 0)
            {
                out_preset = MaterialPreset::VertexColor3D;
                return true;
            }

            if ((key.feature_bits & VF_DebugShading) != 0)
            {
                out_preset = MaterialPreset::Gizmo3D;
                return true;
            }

            out_preset = MaterialPreset::PureColor3D;
            return true;
        }
    }

    if (key.surface_type == SurfaceType::Terrain)
    {
        out_preset = MaterialPreset::TerrainGrid;
        return true;
    }

    if (key.surface_type == SurfaceType::Sky)
    {
        out_preset = MaterialPreset::SkyMinimal;
        return true;
    }

    return false;
}

const char *GetMaterialPresetName(const MaterialPreset mtl_id)
{
    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return "VertexColor2D";
        case MaterialPreset::PureColor2D:           return "PureColor2D";
        case MaterialPreset::PureTexture2D:         return "PureTexture2D";
        case MaterialPreset::RectTexture2D:         return "RectTexture2D";
        case MaterialPreset::RectTexture2DArray:    return "RectTexture2DArray";
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
        case MaterialPreset::StandardTextureArray:  return "StandardTextureArray";
        case MaterialPreset::PBRColor3D:            return "PBRColor3D";
        default:                                    return nullptr;
    }
}

namespace {

using VariantFactory = MaterialCreateInfo*(*)(const contract::PhysicalDeviceProfileLite*, MaterialCreateConfig*);

static MaterialCreateInfo* F_VertexColor2D      (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateVertexColor2D      (p,(const Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_PureColor2D        (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreatePureColor2D        (p,(Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_PureTexture2D      (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreatePureTexture2D      (p,(const Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_RectTexture2D      (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateRectTexture2D      (p,(Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_RectTexture2DArray (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateRectTexture2DArray (p,(Material2DCreateConfig*)c); }
static MaterialCreateInfo* F_Text2D             (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateText2D             (p,(const Text2DMaterialCreateConfig*)c); }
static MaterialCreateInfo* F_PureColor3D        (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreatePureColor3D        (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexColor3D      (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateVertexColor3D      (p,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexLuminance3D  (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateVertexLuminance3D  (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexLuminance2D  (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateVertexLuminance2D  (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_VertexPattleColor3D(const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateVertexPattleColor3D(p,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_Gizmo3D            (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateGizmo3D            (p,(Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_TerrainGrid        (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateTerrainGrid        (p,(const TerrainGridCreateConfig*)c); }
static MaterialCreateInfo* F_SkyMinimal         (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateSkyMinimal         (p,(const SkyMinimalCreateConfig*)c); }
static MaterialCreateInfo* F_Billboard2D        (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateBillboard2D        (p,(BillboardMaterialCreateConfig*)c); }
static MaterialCreateInfo* F_Standard           (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateStandard           (p,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_StandardTextureArray(const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreateStandardTextureArray(p,(const Material3DCreateConfig*)c); }
static MaterialCreateInfo* F_PBRColor3D         (const contract::PhysicalDeviceProfileLite* p, MaterialCreateConfig* c) { return CreatePBRColor3D         (p,(PBRColor3DMaterialCreateConfig*)c); }

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
    reg(MaterialPreset::RectTexture2DArray,   F_RectTexture2DArray);
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
    reg(MaterialPreset::StandardTextureArray, F_StandardTextureArray);
    reg(MaterialPreset::PBRColor3D,           F_PBRColor3D);
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
    if(!cfg)
        return nullptr;
    const auto &m = GetFactoryMap();
    auto it = m.find(key.Hash());
    if(it == m.end())
        return nullptr;
    return it->second(profile, cfg);
}

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    return CreateMaterialCreateInfo(profile, MapPresetToVariantKey(mtl_id), cfg);
}

}//namespace hgl::graph::mtl
