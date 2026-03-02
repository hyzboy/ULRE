#pragma once

#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/mtl/MaterialCreateConfig.h>
#include<hgl/graph/data/CoordinateSystem.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{
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

    std::string ToHashStdString() override;
};//struct Material2DCreateConfig:public MaterialCreateConfig

DECLARE_MATERIAL_CREATOR(VertexColor2D,         const Material2DCreateConfig)
DECLARE_MATERIAL_CREATOR(PureColor2D,           Material2DCreateConfig)

DECLARE_MATERIAL_CREATOR(PureTexture2D,         const Material2DCreateConfig)
DECLARE_MATERIAL_CREATOR(RectTexture2D,         Material2DCreateConfig)
DECLARE_MATERIAL_CREATOR(RectTexture2DArray,    Material2DCreateConfig)

struct Text2DMaterialCreateConfig:public Material2DCreateConfig
{
public:

    Text2DMaterialCreateConfig():Material2DCreateConfig(PrimitiveType::SolidRectangles,CoordinateSystem2D::Ortho,WithLocalToWorld::Without)
    {
        material_instance=true;        //包含材质实例

        position_format=VAT_IVEC2;
    }
};

DECLARE_MATERIAL_CREATOR(Text2D, const Text2DMaterialCreateConfig)

}//namespace hgl::graph::mtl
