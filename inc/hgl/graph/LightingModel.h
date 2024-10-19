#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

enum class LightingModel:uint8
{
    Unlit,

    Gizmo,          ///<Gizmo专用(Blinnphong的特定版本，内置假的太阳光方向、高光系数等，使其不需要外部UBO传入)

    Blinnphong,     ///<Blinnphong光照模型

    FakePBR,        ///<假PBR(使用Blinnphong+HalfLambert模拟)
    MicroPBR,       ///<微型PBR(只有BaseColor/Normal/Metallic/Roughness四个基础数据的PBR)

    WebPBR,         ///<Khronos为WebGL提供的PBR
    FilamentPBR,    ///<Filament引擎所提供的PBR
    AMDPBR,         ///<AMD Caulrdon框架所提供的PBR

    BlenderPBR,     ///<Blender所提供的PBR

    ENUM_CLASS_RANGE(Unlit,BlenderPBR)
};//enum class LightingModel:uint8

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
    GradientColor,      ///<过渡色
    Cubemap,            ///<立方体贴图
    IBL,                ///<IBL立方体贴图

    ENUM_CLASS_RANGE(PureColor,IBL)
};//enum class SkyLightSource:uint8

/**
 * 环境光来源
 */
enum class AmbientLightSource:uint8
{
    PureColor,          ///<纯色

    Distance,           ///<距离(用于显示深邃场景，比如峡谷、深井、管道，越远越黑。理论等同AO)

    LightProbe,         ///<光线探针(也许也是Cubemap，也许不是)

    Cubemap,            ///<本地Cubemap

};//enum class AmbientLightSource:uint8

VK_NAMESPACE_END
