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
        rt_output.color=1;
        rt_output.depth=false;
        rt_output.stencil=false;

        coordinate_system=CoordinateSystem2D::NDC;
        local_to_world=false;
    }
};//struct Material2DConfig:public MaterialConfig
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_2D_CONFIG_INCLUDE
