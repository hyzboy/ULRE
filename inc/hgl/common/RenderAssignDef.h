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
        constexpr const char *      VIS_NAME        = "TransformID";
        constexpr VAType            VAT_FMT         = VAT_UINT;
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(ValueType);
    }

    namespace DataIndexID
    {
        using ValueType = uint16_t;
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R16_UINT;
        constexpr const char *      VIS_NAME        = "DataIndexID";
        constexpr VAType            VAT_FMT         = VAT_UINT;
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(ValueType);
    }

    namespace TextureLayerID
    {
        using ValueType = uint16_t;
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R16_UINT;
        constexpr const char *      VIS_NAME        = "TextureLayerID";
        constexpr VAType            VAT_FMT         = VAT_UINT;
        constexpr const uint32_t    STRIDE_BYTES    = sizeof(ValueType);
    }

    namespace MaterialInstanceID
    {
        // Legacy alias (Phase 3 compatibility): equals DataIndexID.
        using ValueType = DataIndexID::ValueType;
        constexpr VkFormat          VAB_FMT         = DataIndexID::VAB_FMT;
        constexpr const char *      VIS_NAME        = "MaterialInstanceID";
        constexpr VAType            VAT_FMT         = DataIndexID::VAT_FMT;
        constexpr const uint32_t    STRIDE_BYTES    = DataIndexID::STRIDE_BYTES;
    }
}
