#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class DescriptorSetType:int
    {
        Unknow=-1,

        Scene=0,
        Transform,
        Material,
        VertexData,
        Bindless,           ///< 全局 Bindless 纹理数组集合（Set 4）

        ENUM_CLASS_RANGE(Scene,Bindless)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    constexpr const char *DescriptSetTypeName[]=
    {
        "Scene",
        "Transform",
        "Material",
        "VertexData",
        "Bindless"
    };

    inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
    {
        if(type==DescriptorSetType::Unknow)return "Unknow";

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

        // Legacy aliases
        if(!hgl::strcmp(str,"RenderTarget"))
            return DescriptorSetType::Scene;

        if(!hgl::strcmp(str,"Camera"))
            return DescriptorSetType::Scene;

        if(!hgl::strcmp(str,"View"))
            return DescriptorSetType::Scene;

        if(!hgl::strcmp(str,"Global"))
            return DescriptorSetType::Scene;

        if(!hgl::strcmp(str,"World"))
            return DescriptorSetType::Scene;

        if(!hgl::strcmp(str,"Static"))
            return DescriptorSetType::Scene;

        if(!hgl::strcmp(str,"PerFrame"))
            return DescriptorSetType::Transform;

        if(!hgl::strcmp(str,"Draw"))
            return DescriptorSetType::Transform;

        if(!hgl::strcmp(str,"Instance"))
            return DescriptorSetType::Transform;

        if(!hgl::strcmp(str,"PerMaterial"))
            return DescriptorSetType::Material;

        return(DescriptorSetType::Unknow);
    }
}
