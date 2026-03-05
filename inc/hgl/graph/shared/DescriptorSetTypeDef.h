#pragma once

#include <hgl/vk/VKNamespace.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class DescriptorSetType
    {
        Unknow=0,

        RenderTarget,

        Camera,

        World,

        Global,
        PerFrame,
        PerMaterial,

        ENUM_CLASS_RANGE(Unknow,PerMaterial)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    constexpr const char *DescriptSetTypeName[]=
    {
        "Unknow",

        "RenderTarget",

        "Camera",

        "World",

        "Global",
        "PerFrame",
        "PerMaterial"
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

        if(!hgl::strcmp(str,"Scene"))
            return DescriptorSetType::Global;

        if(!hgl::strcmp(str,"View"))
            return DescriptorSetType::Camera;

        if(!hgl::strcmp(str,"Draw"))
            return DescriptorSetType::PerFrame;

        if(!hgl::strcmp(str,"Material"))
            return DescriptorSetType::PerMaterial;

        if(!hgl::strcmp(str,"Static"))
            return DescriptorSetType::World;

        if(!hgl::strcmp(str,"Instance"))
            return DescriptorSetType::PerFrame;

        return(DescriptorSetType::Unknow);
    }
}
