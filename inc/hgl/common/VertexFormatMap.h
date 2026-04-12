#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/vk/VKFormat.h>

#include <absl/container/btree_map.h>

// Semantic aliases for compact normal/tangent storage.
// Prefer these names over raw PF_A2RGB10SN / PF_A2BGR10SN in handwritten code.
#define HGL_NT_PACK_FMT_A2RGB10_SNORM PF_NORMAL_32BITS_A2RGB10
#define HGL_NT_PACK_FMT_A2BGR10_SNORM PF_NORMAL_32BITS_A2BGR10
#define HGL_NT_PACK_FMT_DEFAULT_SNORM PF_NORMAL_MID
#define HGL_NT_PACK_FMT_IS_1010102_SNORM(fmt) (((fmt) == HGL_NT_PACK_FMT_A2RGB10_SNORM) || ((fmt) == HGL_NT_PACK_FMT_A2BGR10_SNORM))
#define HGL_NT_PACK_FMT_IS_A2RGB10(fmt) ((fmt) == HGL_NT_PACK_FMT_A2RGB10_SNORM)
#define HGL_NT_PACK_FMT_IS_A2BGR10(fmt) ((fmt) == HGL_NT_PACK_FMT_A2BGR10_SNORM)

namespace hgl::graph
{
    using VertexFormatMap = absl::btree_map<VertexAttrib, VkFormat>;

    namespace vfmt
    {
        inline constexpr VkFormat kNormalTangentPackedA2RGB10SNorm = PF_NORMAL_32BITS_A2RGB10;
        inline constexpr VkFormat kNormalTangentPackedA2BGR10SNorm = PF_NORMAL_32BITS_A2BGR10;
        inline constexpr VkFormat kNormalTangentPackedDefaultSNorm = PF_NORMAL_MID;

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

        // Mobile low-quality preset:
        // - 2-channel compressed normal (PF_NORMAL_LOW)
        // - no tangent attribute
        // - half UV
        inline const VertexFormatMap kLitSurfaceN_Low_NoTangent_UV_HF16x2 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_NORMAL_LOW},
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
            {VAN::Normal, kNormalTangentPackedA2BGR10SNorm},
            {VAN::Tangent, kNormalTangentPackedA2BGR10SNorm},
            {VAN::TexCoord, PF_RG16F},
        };

        inline const VertexFormatMap kLitSurfaceNT_PackedDefaultSNorm_UV_HF16x2 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, PF_NORMAL_MID},
            {VAN::Tangent, PF_TANGENT_MID},
            {VAN::TexCoord, PF_RG16F},
        };

        inline const VertexFormatMap kLitSurfaceNT_A2RGB10SN_A2RGB10SN_UV_HF16x2 = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal, kNormalTangentPackedA2RGB10SNorm},
            {VAN::Tangent, kNormalTangentPackedA2RGB10SNorm},
            {VAN::TexCoord, PF_RG16F},
        };
    }
}