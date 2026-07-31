#pragma once

#include<hgl/mtl/SkyLight.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include<hgl/graph/ssbo/StandardMaterialInstance.h>
#include<hgl/graph/ssbo/StandardTextureArrayMaterialInstance.h>
#include<hgl/graph/ssbo/PBRColor3DMaterialInstance.h>
#include<vulkan/vulkan.h>

namespace hgl::graph::mtl{

constexpr float DefaultNormalStrength = ::hgl::graph::ssbo::kDefaultMaterialNormalStrength;

struct Material3DCreateConfig:public MaterialCreateConfig
{
    bool                camera;                 ///<包含摄像机矩阵信息

    bool                sky;                    ///<是否包含天空信息(主要是太阳光和大气散射相关)

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

        return std::strong_ordering::equal;
    }

    std::string ToHashStdString() override;
};//struct Material3DCreateConfig:public MaterialCreateConfig

DECLARE_MATERIAL_CREATOR(PureColor3D,       Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexColor3D,     const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexLuminance3D, Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexPattleColor3D,const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(Gizmo3D,           Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(Standard,          const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(StandardTextureArray,const Material3DCreateConfig)

struct SkyMinimalCreateConfig:public Material3DCreateConfig
{
public:

    SkyMinimalCreateConfig(const WithCamera &wc=WithCamera::With)
        :Material3DCreateConfig(PrimitiveType::Triangles,wc,WithLocalToWorld::With,WithSky::With)
    {
    }
};

DECLARE_MATERIAL_CREATOR(SkyMinimal,        const SkyMinimalCreateConfig)

using StandardMaterialInstance = ::hgl::graph::ssbo::StandardMaterialInstance;
constexpr const size_t StandardMaterialInstanceBytes = ::hgl::graph::ssbo::StandardMaterialInstanceBytes;

using StandardTextureArrayMaterialInstance = ::hgl::graph::ssbo::StandardTextureArrayMaterialInstance;
constexpr const size_t StandardTextureArrayMaterialInstanceBytes = ::hgl::graph::ssbo::StandardTextureArrayMaterialInstanceBytes;

using PBRColor3DMaterialInstance = ::hgl::graph::ssbo::PBRColor3DMaterialInstance;
constexpr const size_t PBRColor3DMaterialInstanceBytes = ::hgl::graph::ssbo::PBRColor3DMaterialInstanceBytes;

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
