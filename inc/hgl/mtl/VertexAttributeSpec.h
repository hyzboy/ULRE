#pragma once

#include <hgl/common/InterpolationDef.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

constexpr uint8 AUTO_VERTEX_ATTRIBUTE_LOCATION = 0xFF;

struct VertexAttributeSpec
{
    VertexAttrib         attrib;
    VAType               shader_type;
    VkFormat             storage_format = VK_FORMAT_UNDEFINED;
    uint8                location = AUTO_VERTEX_ATTRIBUTE_LOCATION;
    VkVertexInputRate    input_rate = VK_VERTEX_INPUT_RATE_VERTEX;
    Interpolation        interpolation = Interpolation::Smooth;
};

inline bool HasExplicitVertexLocation(const VertexAttributeSpec &spec)
{
    return spec.location != AUTO_VERTEX_ATTRIBUTE_LOCATION;
}

inline VertexAttributeSpec MakeLegacyVertexAttributeSpec(const VAType &type,const VertexAttrib attrib)
{
    VertexAttributeSpec spec{};

    spec.attrib = attrib;
    spec.shader_type = type;
    spec.storage_format = GetVulkanFormat(&type);

    return spec;
}

}//namespace hgl::graph::mtl