#ifndef HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE
#define HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN
struct Material2DCreateConfig:public MaterialCreateConfig,public Comparator<Material2DCreateConfig>
{
    CoordinateSystem2D  coordinate_system;      ///<使用的坐标系

    bool                local_to_world;         ///<包含LocalToWorld矩阵

    VAType              position_format;        ///<position格式

public:

    Material2DCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name,const PrimitiveType &p):MaterialCreateConfig(da,name,p)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=false;      //不输出深度
        rt_output.stencil=false;    //不输出stencil

        coordinate_system=CoordinateSystem2D::NDC;
        local_to_world=false;

        if(prim==PrimitiveType::SolidRectangles
         ||prim==PrimitiveType::WireRectangles)
            position_format=VAT_VEC4;
        else
            position_format=VAT_VEC2;
    }

    const int compare(const Material2DCreateConfig &cfg)const override
    {
        int off=MaterialCreateConfig::compare(cfg);

        if(off)return off;

        off=(int)coordinate_system-(int)cfg.coordinate_system;
        if(off)return off;

        off=local_to_world-cfg.local_to_world;
        if(off)return off;

        off=position_format.Comp(cfg.position_format);
       
        return off;
    }
};//struct Material2DCreateConfig:public MaterialCreateConfig

MaterialCreateInfo *CreateVertexColor2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreatePureColor2D(const Material2DCreateConfig *);

struct MaterialLerpLineConfig
{

};

MaterialCreateInfo *CreateLerpLine2D(const Material2DCreateConfig *);

MaterialCreateInfo *CreatePureTexture2D(const Material2DCreateConfig *);
MaterialCreateInfo *CreateRectTexture2D(Material2DCreateConfig *);
MaterialCreateInfo *CreateRectTexture2DArray(Material2DCreateConfig *);

// 为什么有了LoadMaterialFromFile，还需要保留以上Create系列函数？

//  1.LoadMaterialFromFile载的材质，是从文件中加载的。但我们要考虑文件损坏不能加载的情况。
//  2.从文件加载材质过于复杂，而且不利于调试。所以我们需要保留Create系列函数，以便于调试以及测试一些新的情况。同时让开发人员知道材质具体是如何创建的。

/**
 * 从文件加载材质
 * @param mtl_name 材质名称
 * @param cfg 材质创建参数
 * @return 材质创建信息
 */
MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &mtl_name,Material2DCreateConfig *cfg);        ///<从文件加载材质
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_2D_CREATE_CONFIG_INCLUDE
