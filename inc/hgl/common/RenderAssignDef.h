#pragma once

#include <hgl/common/RenderOptions.h>
#include <hgl/common/VertexAttribDef.h>
#include <vulkan/vulkan.h>
#include <cstdint>

namespace hgl::graph::Assign
{
    namespace TransformID
    {
    #if defined(HGL_TRANSFORM_ID_U32) && HGL_TRANSFORM_ID_U32
        using ValueType = uint32_t;
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R32_UINT;
    #else
        using ValueType = uint16_t;
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R16_UINT;
    #endif
        constexpr VertexSemantic    VIS_SEMANTIC    = VertexSemantic::TransformID;
        constexpr VAType            VAT_FMT         = VAT_UINT;
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(ValueType);
    }

    namespace DataIndexID
    {
        using ValueType = uint16_t;
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R16_UINT;
        constexpr VertexSemantic    VIS_SEMANTIC    = VertexSemantic::DataIndexID;
        constexpr VAType            VAT_FMT         = VAT_UINT;
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(ValueType);
    }

}
