#pragma once

#include <hgl/common/RenderOptions.h>
#include <hgl/common/VertexAttribDef.h>
#include <vulkan/vulkan.h>
#include <cstdint>

namespace hgl::graph::Assign
{
    namespace TransformID
    {
        // TransformID 强制 uint32（R32_UINT）——与 l2w_index_rows 调色板 uint32
        // 一致，SSBO 直读 uint 无错位（历史 R16 分支已移除）。
        using ValueType = uint32_t;
        constexpr VkFormat          VAB_FMT         = VK_FORMAT_R32_UINT;
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
