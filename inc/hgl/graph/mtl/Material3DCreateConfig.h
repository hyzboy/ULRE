#pragma once

#include<hgl/graph/mtl/MaterialConfig.h>
#include<hgl/graph/CoordinateSystem.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN
enum class LightingModel:uint8
{
    Unlit,

    Gizmo,          ///<Gizmo专用(Blinnphong的特定版本，内置假的太阳光方向、高光系数等，使其不需要外部UBO传入)

    Blinnphong,     ///<Blinnphong光照模型

    FakePBR,        ///<假PBR(使用Blinnphong+HalfLambert模拟)
    MicroPBR,       ///<微型PBR(只有BaseColor/Normal/Metallic/Roughness四个基础数据的PBR)

    WebPBR,         ///<Khronos为WebGL提供的PBR
    FilamentPBR,    ///<Filament引擎所使用的PBR
    AMDPBR,         ///<AMD Caulrdon框架所使用的PBR

    BlenderPBR,     ///<Blender所使用的PBR

    ENUM_CLASS_RANGE(Unlit,BlenderPBR)
};

constexpr const char *LightingModelName[]=
{
    "Unlit",

    "Gizmo",

    "Blinnphong",

    "FakePBR",
    "MicroPBR",

    "WebPBR",
    "FilamentPBR",
    "AMDPBR",

    "BlenderPBR"
};

/**
* 天光来源
*/
enum class SkyLightSource:uint8
{
    PureColor,          ///<纯色
    Simplest,           ///<极简(一行代码)
    Cubemap,            ///<立方体贴图
    IBL,                ///<IBL立方体贴图

    ENUM_CLASS_RANGE(PureColor,IBL)
};

struct Material3DCreateConfig:public MaterialCreateConfig
{
    bool                camera;                 ///<包含摄像机矩阵信息

    bool                local_to_world;         ///<包含LocalToWorld矩阵

    VAType              position_format;        ///<position格式

//    bool                reverse_depth;          ///<使用反向深度

public:

    Material3DCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name,const Prim &p):MaterialCreateConfig(da,name,p)
    {
        rt_output.color=1;          //输出一个颜色
        rt_output.depth=true;       //不输出深度
        rt_output.stencil=false;    //不输出stencil

        camera=true;
        local_to_world=false;

        position_format=VAT_VEC3;

//        reverse_depth=false;
    }

    int Comp(const Material3DCreateConfig &cfg)const
    {
        int off=MaterialCreateConfig::Comp(cfg);

        if(off)return off;

        off=camera-cfg.camera;
        if(off)return off;

        off=local_to_world-cfg.local_to_world;
        if(off)return off;

        off=position_format.Comp(cfg.position_format);

        return off;
    }

    CompOperator(const Material3DCreateConfig &,Comp)
};//struct Material3DCreateConfig:public MaterialCreateConfig

MaterialCreateInfo *CreateVertexColor3D(const Material3DCreateConfig *);
MaterialCreateInfo *CreateVertexLuminance3D(const Material3DCreateConfig *);

MaterialCreateInfo *CreateMaterialGizmo3D(const Material3DCreateConfig *cfg);

struct BillboardMaterialCreateConfig:public Material3DCreateConfig
{
    bool        fixed_size;             ///<固定大小(指像素尺寸)

    Vector2u    pixel_size;             ///<像素尺寸

public:

    using Material3DCreateConfig::Material3DCreateConfig;
};

MaterialCreateInfo *CreateBillboard2D(mtl::BillboardMaterialCreateConfig *);

/**
 * 从文件加载材质
 * @param mtl_name 材质名称
 * @param cfg 材质创建参数
 * @return 材质创建信息
 */
MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,Material3DCreateConfig *cfg);
STD_MTL_NAMESPACE_END
