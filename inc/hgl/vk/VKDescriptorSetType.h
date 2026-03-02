#pragma once

#include<hgl/vk/VKNamespace.h>
#include<hgl/type/StrChar.h>
#include<hgl/type/EnumUtil.h>

namespace hgl::graph{

/**
* <summary>
*
*    layout(location=?) in uint MaterialInstanceID
*
*    #define MI_MAX_COUNT ???                //该值由引擎根据 UBORange/sizeof(MaterialInstance) 计算出来
*
*    struct MaterialInstance                 //这部分数据，即为材质实例的具体数据，每一个材质实例类负责提供具体数据。由RenderCollector合并成一整个UBO
*    {                                       //该类数据，由DescriptorSetType为PerMaterial的参数构成
*        vec4 BaseColor;
*        vec4 Emissive;
*        uvec4 ARM;
*    };
*
*    layout(set=?,binding=?) uniform Material
*    {
*        MaterialInstance mi[MI_MAX_COUNT]
*    }mtl;
*
*    void main()
*    {
*        MaterialInstance mi=mtl.mi[(MaterialInstanceID>=MI_MAX_COUNT)?:0:MaterialInstanceID];   //如果超出范围则使用0号材质实例数据
*
*        vec4 BaseColor  =mi.BaseColor;
*        vec4 Emissive   =mi.Emissive;
*
*        float AO        =mi.ARM.x;
*        float Roughness =mi.ARM.y;
*        float Metallic  =mi.ARM.z;
*
* </summary>
*/

/**
* 描述符集类型
*/
enum class DescriptorSetType
{
    Unknow=0,           ///<未分类的（不应进入最终布局）

    Scene,              ///<场景级：世界/全局/静态环境参数（低频更新）
    View,               ///<视图级：RenderTarget/Camera/Pass 参数（每相机/每pass更新）
    Draw,               ///<绘制级：PerFrame/Instance（高频更新，通常配合 dynamic buffer）
    Material,           ///<材质级：材质参数、贴图、采样器

    // 兼容旧命名（保持旧代码可编译，后续可逐步迁移到 Scene/View/Draw/Material）
    RenderTarget=View,
    Camera=View,
    World=Scene,
    Static=Scene,
    Global=Scene,
    PerFrame=Draw,
    PerMaterial=Material,
    Instance=Draw,

    ENUM_CLASS_RANGE(Unknow,Material)
};//

constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

constexpr const char *DescriptSetTypeName[]=
{
    "Unknow",

    "Scene",
    "View",
    "Draw",
    "Material"
};

inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
{
    RANGE_CHECK_RETURN_NULLPTR(type);

    return DescriptSetTypeName[(size_t)type];
}

inline const DescriptorSetType GetDescriptorSetType(const char *str)
{
    if(!str||!*str)return(DescriptorSetType::Unknow);

    for(size_t i=0;i<DESCRIPTOR_SET_TYPE_COUNT;i++)
    {
        if(!hgl::strcmp(str,DescriptSetTypeName[i]))
            return((DescriptorSetType)i);
    }

    // legacy names compatibility
    if(!hgl::strcmp(str,"RenderTarget")||!hgl::strcmp(str,"Camera"))
        return DescriptorSetType::View;

    if(!hgl::strcmp(str,"World")||!hgl::strcmp(str,"Static")||!hgl::strcmp(str,"Global"))
        return DescriptorSetType::Scene;

    if(!hgl::strcmp(str,"PerFrame")||!hgl::strcmp(str,"Instance"))
        return DescriptorSetType::Draw;

    if(!hgl::strcmp(str,"PerMaterial"))
        return DescriptorSetType::Material;

    return(DescriptorSetType::Unknow);
}

}//namespace hgl::graph
