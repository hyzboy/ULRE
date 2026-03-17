#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>

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

MaterialCreateInfo *CreateMaterialCreateInfo(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return CreateVertexColor2D      (profile,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::PureColor2D:           return CreatePureColor2D        (profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::PureTexture2D:         return CreatePureTexture2D      (profile,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::RectTexture2D:         return CreateRectTexture2D      (profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::RectTexture2DArray:    return CreateRectTexture2DArray (profile,(Material2DCreateConfig *)cfg);
        case MaterialPreset::Text2D:                return CreateText2D             (profile,(const Text2DMaterialCreateConfig *)cfg);

        case MaterialPreset::PureColor3D:           return CreatePureColor3D        (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexColor3D:         return CreateVertexColor3D      (profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexLuminance3D:     return CreateVertexLuminance3D  (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexLuminance2D:     return CreateVertexLuminance2D  (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexPattleColor3D:   return CreateVertexPattleColor3D(profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::Gizmo3D:               return CreateGizmo3D            (profile,(Material3DCreateConfig *)cfg);
        case MaterialPreset::TerrainGrid:           return CreateTerrainGrid        (profile,(const TerrainGridCreateConfig *)cfg);
        case MaterialPreset::SkyMinimal:            return CreateSkyMinimal         (profile,(const SkyMinimalCreateConfig *)cfg);
        case MaterialPreset::Billboard2D:           return CreateBillboard2D        (profile,(BillboardMaterialCreateConfig *)cfg);
        case MaterialPreset::Standard:              return CreateStandard           (profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::StandardTextureArray:  return CreateStandardTextureArray(profile,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::PBRColor3D:            return CreatePBRColor3D         (profile,(PBRColor3DMaterialCreateConfig *)cfg);

        default:                                    return nullptr;
    }
}
}//namespace hgl::graph::mtl
