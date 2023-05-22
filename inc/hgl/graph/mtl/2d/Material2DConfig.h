#ifndef HGL_GRAPH_MTL_2D_CONFIG_INCLUDE
#define HGL_GRAPH_MTL_2D_CONFIG_INCLUDE

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>

STD_MTL_NAMESPACE_BEGIN
struct Material2DConfig:public MaterialConfig
{
    CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

    bool                local_to_world;         ///<包含LocalToWorld矩阵

public:

    Material2DConfig(const AnsiString &name):MaterialConfig(name)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=false;      //不输出深度
        rt_output.stencil=false;    //不输出stencil

        coordinate_system=CoordinateSystem2D::NDC;
        local_to_world=false;
    }
};//struct Material2DConfig:public MaterialConfig

MaterialCreateInfo *CreateVertexColor2D(const Material2DConfig *);
MaterialCreateInfo *CreatePureColor2D(const Material2DConfig *);
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_2D_CONFIG_INCLUDE
