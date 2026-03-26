#pragma once

#include<hgl/mtl/SkyLight.h>
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

//    bool                reverse_depth;          ///<使用反向深度

public:

    Material3DCreateConfig(const PrimitiveType &    p   =PrimitiveType::Triangles,
                           const WithCamera &       wc  =WithCamera::With,
                           const WithLocalToWorld & l2w =WithLocalToWorld::With,
                           const WithSky &          s   =WithSky::Without)
        :MaterialCreateConfig(p,l2w==WithLocalToWorld::With)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=true;       //输出深度
        rt_output.stencil=false;    //不输出stencil

        camera=(wc==WithCamera::With);

        sky=(s==WithSky::With);

        position_format=VAT_VEC3;

        sky_ambient_model=SkyLightAmbientModel::Simple;

//        reverse_depth=false;
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

        return position_format <=> cfg.position_format;
    }

    std::string ToHashStdString() override;
};//struct Material3DCreateConfig:public MaterialCreateConfig

DECLARE_MATERIAL_CREATOR(PureColor3D,       Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexColor3D,     const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexLuminance3D, Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexLuminance2D, Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexPattleColor3D,const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(Gizmo3D,           Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(Standard,          const Material3DCreateConfig)

struct TerrainGridCreateConfig:public Material3DCreateConfig
{
public:

    TerrainGridCreateConfig()
        :Material3DCreateConfig(PrimitiveType::Triangles,
                                WithCamera::With,WithLocalToWorld::With,WithSky::With)
    {
    }
};

DECLARE_MATERIAL_CREATOR(TerrainGrid,       const TerrainGridCreateConfig)

struct SkyMinimalCreateConfig:public Material3DCreateConfig
{
public:

    SkyMinimalCreateConfig(const WithCamera &wc=WithCamera::With)
        :Material3DCreateConfig(PrimitiveType::Triangles,wc,WithLocalToWorld::With,WithSky::With)
    {
    }
};

DECLARE_MATERIAL_CREATOR(SkyMinimal,        const SkyMinimalCreateConfig)

struct BillboardMaterialCreateConfig:public Material3DCreateConfig
{
    bool        fixed_size;             ///<固定大小(指像素尺寸)

    Vector2u    pixel_size;             ///<像素尺寸

    VkFrontFace front_face=VK_FRONT_FACE_CLOCKWISE; ///<正面朝向

    std::string texture_id;             ///<纹理唯一标识，用于区分不同纹理的材质缓存

    BlendMode   blend_mode = BlendMode::Transparent; ///<透明/混合模式，决定shader variant

    TextureChannelHint base_color_channel = TextureChannelHint::RGBA; ///<BaseColor纹理通道提示

public:

    using Material3DCreateConfig::Material3DCreateConfig;

    std::string ToHashStdString() override;
};

DECLARE_MATERIAL_CREATOR(Billboard2D, BillboardMaterialCreateConfig)

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
                                WithCamera::With,
                                WithLocalToWorld::With,
                                WithSky::With)
    {}
};

DECLARE_MATERIAL_CREATOR(PBRColor3D, PBRColor3DMaterialCreateConfig)

}//namespace hgl::graph::mtl
