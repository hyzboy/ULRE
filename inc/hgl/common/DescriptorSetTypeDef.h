#pragma once

#include <vulkan/vulkan.h>
#include <hgl/type/StrChar.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class DescriptorSetType:int
    {
        Unknow=-1,

        Static=0,
        PerFrame,
        PerObject,
        PerMaterial,
        VertexStreams,          ///< Per-vertex attribute SSBO streams (Phase B)

        ENUM_CLASS_RANGE(Static,VertexStreams)
    };

    constexpr const size_t DESCRIPTOR_SET_TYPE_COUNT=size_t(DescriptorSetType::RANGE_SIZE);

    constexpr const char *DescriptSetTypeName[]=
    {
        "Static",
        "PerFrame",
        "PerObject",
        "PerMaterial",
        "VertexStreams"
    };

    inline const char *GetDescriptorSetTypeName(const enum class DescriptorSetType &type)
    {
        if(type==DescriptorSetType::Unknow)return "Unknow";

        RANGE_CHECK_RETURN_NULLPTR(type);

        return DescriptSetTypeName[(size_t)type];
    }

    // Resource-level aliases for readability in use-sites.
    constexpr DescriptorSetType SET_TYPE_VIEWPORT       = DescriptorSetType::Static;
    constexpr DescriptorSetType SET_TYPE_SKY            = DescriptorSetType::Static;
    constexpr DescriptorSetType SET_TYPE_CAMERA         = DescriptorSetType::PerFrame;
    constexpr DescriptorSetType SET_TYPE_TRANSFORM      = DescriptorSetType::PerObject;
    constexpr DescriptorSetType SET_TYPE_MATERIAL       = DescriptorSetType::PerMaterial;
    constexpr DescriptorSetType SET_TYPE_TEXTURE        = DescriptorSetType::PerMaterial;
    constexpr DescriptorSetType SET_TYPE_VERTEX_STREAMS = DescriptorSetType::VertexStreams;
}
