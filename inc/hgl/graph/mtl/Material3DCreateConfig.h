#include <string>
#pragma once

#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/mtl/MaterialCreateConfig.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

struct Material3DCreateConfig:public MaterialCreateConfig
{
    bool                camera;                 ///<包含摄像机矩阵信息

    bool                sky;                    ///<是否包含天空信息(主要是太阳光和大气散射相关)

    VAType              position_format;        ///<position格式

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

        return position_format <=> cfg.position_format;
    }

    const std::string ToHashString() override;
};//struct Material3DCreateConfig:public MaterialCreateConfig

DECLARE_MATERIAL_CREATOR(PureColor3D,       Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexColor3D,     const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexLuminance3D, Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(VertexPattleColor3D,const Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(Gizmo3D,           Material3DCreateConfig)
DECLARE_MATERIAL_CREATOR(TextureBlinnPhong, const Material3DCreateConfig)

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

public:

    using Material3DCreateConfig::Material3DCreateConfig;
};

DECLARE_MATERIAL_CREATOR(Billboard2D, BillboardMaterialCreateConfig)

struct BasicLitMaterialInstance
{
    uint32 base_color;      ///<基础颜色
    float  metallic;        ///<金属度
    float  roughness;       ///<粗糙度
    float  fresnel;         ///<菲涅尔反射系数
    float  ibl_intensity;   ///<IBL强度
};

constexpr const size_t BasicLitMaterialInstanceBytes=sizeof(BasicLitMaterialInstance);

struct BasicLitMaterialCreateConfig:public Material3DCreateConfig
{
    bool        ibl;                    ///<包含IBL信息

public:

    BasicLitMaterialCreateConfig(const bool use_ibl=false)
        :Material3DCreateConfig(PrimitiveType::Triangles,
                                WithCamera::With,
                                WithLocalToWorld::With,
                                WithSky::With)
    {
        ibl=use_ibl;
    }
};

DECLARE_MATERIAL_CREATOR(BasicLit, BasicLitMaterialCreateConfig)

/**
 * 从文件加载材质
 * @param mtl_name 材质名称
 * @param cfg 材质创建参数
 * @return 材质创建信息
 */
MaterialCreateInfo *LoadMaterialFromFile(const VulkanDevAttr *dev_attr,const std::string &name,Material3DCreateConfig *cfg);
}//namespace hgl::graph::mtl
