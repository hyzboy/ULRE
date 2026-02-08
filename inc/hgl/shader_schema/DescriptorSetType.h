#pragma once

#include<hgl/type/StrChar.h>
#include<hgl/type/EnumUtil.h>

namespace hgl::shader_schema
{
/**
* 描述符集类型
*/
enum class DescriptorSetType
{
    Unknow=0,           ///<未分类的

    RenderTarget,       ///<所有的RenderTarget相关数据(包括Viewport，显示器HDR参数等等)

    Camera,             ///<相机相关

    World,              ///<场景世界数据，基本不怎么刷新的的数据(如天空球、太阳/月亮等)

    Static,             ///<静态数据，基本上是不会变的

    Global,             ///<全局参数，不确定什么时候更新，但一般不怎么更新(如太阳光), 不会在RenderCollector中处理刷新

    PerFrame,           ///<固定每帧刷新一次(如摄像机位置等)

    PerMaterial,        ///<材质参数

    Instance,           ///<手动Instance绘制用数据

    ENUM_CLASS_RANGE(Unknow,Instance)
};//

constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

constexpr const char *DescriptSetTypeName[]=
{
    "Unknow",

    "RenderTarget",

    "Camera",

    "World",

    "Static",
    "Global",
    "PerFrame",
    "PerMaterial",
    "Instance"
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

    return(DescriptorSetType::Unknow);
}
}//namespace hgl::shader_schema

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
    using hgl::shader_schema::DescriptorSetType;
    using hgl::shader_schema::DESCRIPTOR_SET_TYPE_COUNT;
    using hgl::shader_schema::DescriptSetTypeName;
    using hgl::shader_schema::GetDescriptorSetTypeName;
    using hgl::shader_schema::GetDescriptorSetType;
}//namespace hgl::graph
