#pragma once

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN
struct Material3DCreateConfig:public MaterialCreateConfig
{
    bool                local_to_world;         ///<包含LocalToWorld矩阵

    VAT                 position_format;        ///<position格式

//    bool                reverse_depth;          ///<使用反向深度

public:

    Material3DCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name,const Prim &p):MaterialCreateConfig(da,name,p)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=true;       //不输出深度
        rt_output.stencil=false;    //不输出stencil

        local_to_world=false;

        position_format=VAT_VEC3;

//        reverse_depth=false;
    }
};//struct Material3DCreateConfig:public MaterialCreateConfig

MaterialCreateInfo *CreateVertexColor3D(const Material3DCreateConfig *);
//MaterialCreateInfo *CreatePureColor2D(const Material2DCreateConfig *);
//MaterialCreateInfo *CreatePureTexture2D(const Material2DCreateConfig *);
//MaterialCreateInfo *CreateRectTexture2D(Material2DCreateConfig *);
//MaterialCreateInfo *CreateRectTexture2DArray(Material2DCreateConfig *);
STD_MTL_NAMESPACE_END
