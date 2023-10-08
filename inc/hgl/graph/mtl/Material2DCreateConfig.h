#ifndef HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE
#define HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN
struct Material2DCreateConfig:public MaterialCreateConfig
{
    CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

    bool                local_to_world;         ///<包含LocalToWorld矩阵

    VAT                 position_format;        ///<position格式

public:

    Material2DCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name,const Prim &p):MaterialCreateConfig(da,name,p)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=false;      //不输出深度
        rt_output.stencil=false;    //不输出stencil

        coordinate_system=CoordinateSystem2D::NDC;
        local_to_world=false;

        if(prim==Prim::SolidRectangles
         ||prim==Prim::WireRectangles)
            position_format=VAT_VEC4;
        else
            position_format=VAT_VEC2;
    }
};//struct Material2DCreateConfig:public MaterialCreateConfig

MaterialCreateInfo *CreateVertexColor2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreatePureColor2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreatePureTexture2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreateRectTexture2D(Material2DCreateConfig *);
MaterialCreateInfo *CreateRectTexture2DArray(Material2DCreateConfig *);

MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &,const Material2DCreateConfig *);
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE
