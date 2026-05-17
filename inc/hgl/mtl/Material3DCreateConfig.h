#pragma once

#include<hgl/mtl/SkyLight.h>
#include<hgl/mtl/LightingModel.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include<hgl/common/TextureSamplerTypeDef.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

constexpr float DefaultNormalStrength = 0.35f;

struct Material3DCreateConfig:public MaterialCreateConfig
{
    bool                camera;                 ///<包含摄像机矩阵信息

    bool                sky;                    ///<是否包含天空信息(主要是太阳光和大气散射相关)

    VAType              position_format;        ///<position格式

    SkyLightAmbientModel sky_ambient_model;     ///<天空环境光实现方式（按材质配置）

    LightingModel       lighting_model;          ///<光照模型（Lambert/BlinnPhong/PBR）
    
    // Phase 2: Effective feature mask (resolved from intent_features in MaterialRecipe)
    // Populated by MaterialAssetLoader via ResolveRecipeIntentFeatureMask before calling ApplyCreateConfigToVariantKey.
    // 0 means no intent_features override (use defaults derived from camera/sky/lighting_model).
    uint64              effective_feature_mask = 0;

public:

    Material3DCreateConfig(const PrimitiveType &    p   =PrimitiveType::Triangles,
                           const IncludeCamera &       wc  =IncludeCamera::With,
                           const IncludeL2W & l2w =IncludeL2W::With,
                           const IncludeSky &          s   =IncludeSky::Without)
        :MaterialCreateConfig(p,l2w==IncludeL2W::With)
    {
        kind = ConfigKind::D3;

        rt_output.color=1;          //输出一个颜色
        rt_output.depth=true;       //输出深度
        rt_output.stencil=false;    //不输出stencil

        camera=(wc==IncludeCamera::With);

        sky=(s==IncludeSky::With);

        position_format=VAT_VEC3;

        sky_ambient_model=SkyLightAmbientModel::Simple;

        lighting_model=LightingModel::Lambert;
    }

    std::strong_ordering operator<=>(const Material3DCreateConfig &cfg)const
    {
        if(auto cmp=MaterialCreateConfig::operator<=>(cfg); cmp!=0)
            return cmp;

        if(auto cmp=camera<=>cfg.camera; cmp!=0)
            return cmp;

        if(auto cmp=sky<=>cfg.sky; cmp!=0)
            return cmp;

        if(auto cmp=sky_ambient_model<=>cfg.sky_ambient_model; cmp!=0)
            return cmp;

        if(auto cmp=lighting_model<=>cfg.lighting_model; cmp!=0)
            return cmp;

        if(auto cmp=position_format<=>cfg.position_format; cmp!=0)
            return cmp;

        return effective_feature_mask <=> cfg.effective_feature_mask;
    }

    std::string ToHashStdString() override;
};//struct Material3DCreateConfig:public MaterialCreateConfig

struct TerrainGridCreateConfig:public Material3DCreateConfig
{
public:

    TerrainGridCreateConfig()
        :Material3DCreateConfig(PrimitiveType::Triangles,
                                IncludeCamera::With,IncludeL2W::With,IncludeSky::With)
    {
    }
};

struct SkyMinimalCreateConfig:public Material3DCreateConfig
{
public:

    SkyMinimalCreateConfig(const IncludeCamera &wc=IncludeCamera::With)
        :Material3DCreateConfig(PrimitiveType::Triangles,wc,IncludeL2W::With,IncludeSky::With)
    {
    }
};

struct StandardMaterialInstance
{
    uint32 base_color;      ///<基础颜色
    float  metallic;        ///<金属度
    float  roughness;       ///<粗糙度
    float  normal_scale = DefaultNormalStrength; ///<法线强度(运行时可调)
};

constexpr const size_t StandardMaterialInstanceBytes=sizeof(StandardMaterialInstance);

struct PBRColor3DMaterialInstance
{
    uint32 base_color;      ///<基础颜色 (RGBA packed, unpackUnorm4x8 in shader)
    float  metallic;        ///<金属度 [0, 1]
    float  roughness;       ///<粗糙度 [0.04, 1]
};

constexpr const size_t PBRColor3DMaterialInstanceBytes = sizeof(PBRColor3DMaterialInstance);

struct PBRColor3DMaterialCreateConfig : public Material3DCreateConfig
{
public:
    PBRColor3DMaterialCreateConfig()
        :Material3DCreateConfig(PrimitiveType::Triangles,
                                IncludeCamera::With,
                                IncludeL2W::With,
                                IncludeSky::With)
    {
        lighting_model = LightingModel::PBR;
    }
};

// ---------------------------------------------------------------------------
// Type-safe downcast helpers — replaces dynamic_cast<> throughout the codebase.
// Defined here (not in MaterialCreateConfig.h) so that the complete inheritance
// relationships are visible and static_cast can be validated by the compiler.
// ---------------------------------------------------------------------------

/// Cast to Material3DCreateConfig* if kind is D3; else nullptr.
inline Material3DCreateConfig *As3D(MaterialCreateConfig *cfg) noexcept
{
    if (!cfg) return nullptr;
    return (cfg->kind == ConfigKind::D3)
        ? static_cast<Material3DCreateConfig *>(cfg) : nullptr;
}

inline const Material3DCreateConfig *As3D(const MaterialCreateConfig *cfg) noexcept
{
    if (!cfg) return nullptr;
    return (cfg->kind == ConfigKind::D3)
        ? static_cast<const Material3DCreateConfig *>(cfg) : nullptr;
}

}//namespace hgl::graph::mtl
