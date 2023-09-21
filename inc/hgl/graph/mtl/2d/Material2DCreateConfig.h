#ifndef HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE
#define HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>

STD_MTL_NAMESPACE_BEGIN
struct Material2DCreateConfig:public MaterialCreateConfig
{
    CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

    bool                local_to_world;         ///<包含LocalToWorld矩阵

public:

    Material2DCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name):MaterialCreateConfig(da,name)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=false;      //不输出深度
        rt_output.stencil=false;    //不输出stencil

        coordinate_system=CoordinateSystem2D::NDC;
        local_to_world=false;
    }
};//struct Material2DCreateConfig:public MaterialCreateConfig

namespace SamplerName
{
    constexpr const char Color[]="TextureColor";
}

MaterialCreateInfo *CreateVertexColor2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreatePureColor2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreatePureTexture2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreateRectTexture2D(const Material2DCreateConfig *);
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE
