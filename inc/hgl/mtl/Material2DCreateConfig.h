#pragma once

#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include<hgl/common/CoordinateSystem.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{
struct Material2DCreateConfig:public MaterialCreateConfig
{
    CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

    VAType              position_format;        ///<position格式

public:

    Material2DCreateConfig(const PrimitiveType &p=PrimitiveType::Lines,
                           const CoordinateSystem2D &cs=CoordinateSystem2D::NDC,
                           const IncludeL2W &l2w=IncludeL2W::Without)
        :MaterialCreateConfig(p,l2w==IncludeL2W::With)
    {
        kind = ConfigKind::D2;

        rt_output.color=1;          //输出一个颜色
        rt_output.depth=false;      //不输出深度
        rt_output.stencil=false;    //不输出stencil

        coordinate_system=cs;

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

struct Text2DMaterialCreateConfig:public Material2DCreateConfig
{
public:

    Text2DMaterialCreateConfig():Material2DCreateConfig(PrimitiveType::Triangles,CoordinateSystem2D::Ortho,IncludeL2W::Without)
    {
        kind = ConfigKind::Text2D;

        material_instance=true;        //包含材质实例

        position_format=VAT_IVEC2;
    }
};

}//namespace hgl::graph::mtl
