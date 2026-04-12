#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKFormat.h>

#include <absl/container/btree_map.h>

namespace hgl::graph
{
    using VertexFormatMap = absl::btree_map<VertexAttrib, VkFormat>;

    namespace vfmt
    {
        // 2D vertex formats
        inline const VertexFormatMap kPosition2 = {
            {VAN::Position, PF_RG32F},
        };

        inline const VertexFormatMap kPosition2TexCoord2 = {
            {VAN::Position, PF_RG32F},
            {VAN::TexCoord, PF_RG16F},
        };

        // 2D UI vertex formats
        inline const VertexFormatMap kUIPosition2I16 = {
            {VAN::Position, PF_RG16I},
        };

        inline const VertexFormatMap kUIPosition2I32 = {
            {VAN::Position, PF_RG32I},
        };

        inline const VertexFormatMap kUIPosition2I16TexCoord2 = {
            {VAN::Position, PF_RG16I},
            {VAN::TexCoord, PF_RG16F},
        };

        inline const VertexFormatMap kUIPosition2I32TexCoord2 = {
            {VAN::Position, PF_RG32I},
            {VAN::TexCoord, PF_RG16F},
        };

        // 3D vertex formats
        inline const VertexFormatMap kPosition3 = {
            {VAN::Position, PF_RGB32F},
        };

        inline const VertexFormatMap kPosition3Color4 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Color, PF_RGBA8UN},
        };

        inline const VertexFormatMap kPosition3Normal3 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_RGB32F},
        };

        inline const VertexFormatMap kLitSurface = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_RGB32F},
            {VAN::Tangent, PF_RGB32F},
            {VAN::TexCoord, PF_RG16F},
        };

        inline const VertexFormatMap kLitSurfaceNT_SN8x4_SN8x4_UV_HF16x2 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_RGBA8SN},
            {VAN::Tangent, PF_RGBA8SN},
            {VAN::TexCoord, PF_RG16F},
        };

        inline const VertexFormatMap kLitSurfaceNT_HF16x4_HF16x4_UV_HF16x2 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_RGBA16F},
            {VAN::Tangent, PF_RGBA16F},
            {VAN::TexCoord, PF_RG16F},
        };

        inline const VertexFormatMap kLitSurfaceNT_A2BGR10SN_A2BGR10SN_UV_HF16x2 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_A2BGR10SN},
            {VAN::Tangent, PF_A2BGR10SN},
            {VAN::TexCoord, PF_RG16F},
        };
    }
}