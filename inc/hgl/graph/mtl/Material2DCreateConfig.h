#pragma once

#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/mtl/MaterialCreateConfig.h>
#include<hgl/graph/CoordinateSystem.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN
struct Material2DCreateConfig:public MaterialCreateConfig
{
    CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

    VAType              position_format;        ///<position格式

public:

    Material2DCreateConfig(const PrimitiveType &p=PrimitiveType::Lines,
                           const CoordinateSystem2D &cs=CoordinateSystem2D::NDC,
                           const WithLocalToWorld &l2w=WithLocalToWorld::Without)
        :MaterialCreateConfig(p,l2w==WithLocalToWorld::With)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=false;      //不输出深度
        rt_output.stencil=false;    //不输出stencil

        coordinate_system=cs;

        if(prim==PrimitiveType::SolidRectangles
         ||prim==PrimitiveType::WireRectangles)
            position_format=VAT_VEC4;
        else
            position_format=VAT_VEC2;
    }

    std::strong_ordering operator<=>(const Material2DCreateConfig &cfg)const
    {
        if(auto cmp=MaterialCreateConfig::operator<=>(cfg); cmp!=0)
            return cmp;

        if(auto cmp=coordinate_system<=>cfg.coordinate_system; cmp!=0)
            return cmp;

        return position_format <=> cfg.position_format;
    }

    const AnsiString ToHashString() override;
};//struct Material2DCreateConfig:public MaterialCreateConfig

DEFINE_MATERIAL_FACTORY_CLASS(VertexColor2D,        const Material2DCreateConfig)
DEFINE_MATERIAL_FACTORY_CLASS(PureColor2D,          Material2DCreateConfig)
//DEFINE_MATERIAL_FACTORY_CLASS(LerpLine2D,           const Material2DCreateConfig);

DEFINE_MATERIAL_FACTORY_CLASS(PureTexture2D,        const Material2DCreateConfig);
DEFINE_MATERIAL_FACTORY_CLASS(RectTexture2D,        Material2DCreateConfig);
DEFINE_MATERIAL_FACTORY_CLASS(RectTexture2DArray,   Material2DCreateConfig);

struct Text2DMaterialCreateConfig:public Material2DCreateConfig
{
public:

    Text2DMaterialCreateConfig():Material2DCreateConfig(PrimitiveType::SolidRectangles,CoordinateSystem2D::Ortho,WithLocalToWorld::With)
    {
        material_instance=true;        //包含材质实例

        position_format=VAT_IVEC4;

        enableGeometryShader();
    }
};

DEFINE_MATERIAL_FACTORY_CLASS(Text2D,const Text2DMaterialCreateConfig)

// 为什么有了LoadMaterialFromFile，还需要保留以上Create系列函数？

//  1.LoadMaterialFromFile载的材质，是从文件中加载的。但我们要考虑文件损坏不能加载的情况。
//  2.从文件加载材质过于复杂，而且不利于调试。所以我们需要保留Create系列函数，以便于调试以及测试一些新的情况。同时让开发人员知道材质具体是如何创建的。

/**
 * 从文件加载材质
 * @param dev_attr 设备属性
 * @param mtl_name 材质名称
 * @param cfg 材质创建参数
 * @return 材质创建信息
 */
MaterialCreateInfo *LoadMaterialFromFile(const VulkanDevAttr *dev_attr,const AnsiString &mtl_name,Material2DCreateConfig *cfg);        ///<从文件加载材质

STD_MTL_NAMESPACE_END
