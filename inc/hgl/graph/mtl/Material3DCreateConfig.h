#pragma once

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN

struct Material3DCreateConfig:public MaterialCreateConfig,public Comparator<Material3DCreateConfig>
{
    bool                camera;                 ///<包含摄像机矩阵信息

    bool                local_to_world;         ///<包含LocalToWorld矩阵

    VAType              position_format;        ///<position格式

//    bool                reverse_depth;          ///<使用反向深度

public:

    Material3DCreateConfig(const AnsiString &name,const PrimitiveType &p):MaterialCreateConfig(name,p)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=true;       //不输出深度
        rt_output.stencil=false;    //不输出stencil

        camera=true;
        local_to_world=false;

        position_format=VAT_VEC3;

//        reverse_depth=false;
    }

    const int compare(const Material3DCreateConfig &cfg)const override
    {
        int off=MaterialCreateConfig::compare(cfg);

        if(off)return off;

        off=camera-cfg.camera;
        if(off)return off;

        off=local_to_world-cfg.local_to_world;
        if(off)return off;

        off=position_format.Comp(cfg.position_format);

        return off;
    }
};//struct Material3DCreateConfig:public MaterialCreateConfig

MaterialCreateInfo *CreateVertexColor3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *);
MaterialCreateInfo *CreateVertexLuminance3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *);

MaterialCreateInfo *CreateMaterialGizmo3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *cfg);

struct BillboardMaterialCreateConfig:public Material3DCreateConfig
{
    bool        fixed_size;             ///<固定大小(指像素尺寸)

    Vector2u    pixel_size;             ///<像素尺寸

public:

    using Material3DCreateConfig::Material3DCreateConfig;
};

MaterialCreateInfo *CreateBillboard2D(const VulkanDevAttr *dev_attr,mtl::BillboardMaterialCreateConfig *);

/**
 * 从文件加载材质
 * @param mtl_name 材质名称
 * @param cfg 材质创建参数
 * @return 材质创建信息
 */
MaterialCreateInfo *LoadMaterialFromFile(const VulkanDevAttr *dev_attr,const AnsiString &name,Material3DCreateConfig *cfg);
STD_MTL_NAMESPACE_END
