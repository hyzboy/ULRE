// ============================================================================
// MaterialDomainSlots.gen.h —— 材质领域纹理槽规范契约（自动生成代码）
//
// 唯一真源：ShaderLibrary/contract/material_domain_slots.toml
// 由 src/Tools/DomainSlotGen/domain_slot_gen.py 自动生成，请勿手动修改！
// ============================================================================
#pragma once

#include <hgl/mtl/MaterialDomain.h>
#include <hgl/common/TextureSamplerTypeDef.h>

namespace hgl::graph::mtl
{
    namespace generated
    {
        // StandardOpaque —— 通用不透明/半透明硬表面
        // 槽数: 6, 行跨度(stride): 48 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot StandardOpaqueSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true, SamplerType::Sampler2DArray, false, 0 },
            { "roughness", MaterialDomainTextureRole::Roughness, true, SamplerType::Sampler2DArray, false, 0 },
            { "metallic", MaterialDomainTextureRole::Metallic, true, SamplerType::Sampler2DArray, false, 0 },
            { "occlusion", MaterialDomainTextureRole::Occlusion, true, SamplerType::Sampler2DArray, false, 0 },
            { "opacity_mask", MaterialDomainTextureRole::OpacityMask, true, SamplerType::Sampler2DArray, false, 0 },
            { "normal", MaterialDomainTextureRole::Normal, true, SamplerType::Sampler2DArray, false, 2 },
        };
        constexpr int StandardOpaqueSlotCount = static_cast<int>(sizeof(StandardOpaqueSlots) / sizeof(StandardOpaqueSlots[0]));
        constexpr uint32_t StandardOpaqueRowStride = 48u;

        // Foliage —— 植被/树叶/草（风力摆动 + 双面 + 透射）
        // 槽数: 5, 行跨度(stride): 48 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot FoliageSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true, SamplerType::Sampler2DArray, false, 0 },
            { "normal", MaterialDomainTextureRole::Normal, true, SamplerType::Sampler2DArray, false, 2 },
            { "roughness", MaterialDomainTextureRole::Roughness, true, SamplerType::Sampler2DArray, false, 0 },
            { "transmission", MaterialDomainTextureRole::Transmission, true, SamplerType::Sampler2DArray, false, 0 },
            { "opacity_mask", MaterialDomainTextureRole::OpacityMask, true, SamplerType::Sampler2DArray, false, 0 },
        };
        constexpr int FoliageSlotCount = static_cast<int>(sizeof(FoliageSlots) / sizeof(FoliageSlots[0]));
        constexpr uint32_t FoliageRowStride = 48u;

        // Terrain —— 多层混合地形
        // 槽数: 3, 行跨度(stride): 32 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot TerrainSlots[] = {
            { "splat_map", MaterialDomainTextureRole::SplatMap, true, SamplerType::Sampler2DArray, false, 0 },
            { "base_color", MaterialDomainTextureRole::BaseColor, true, SamplerType::Sampler2DArray, false, 0 },
            { "normal", MaterialDomainTextureRole::Normal, true, SamplerType::Sampler2DArray, false, 2 },
        };
        constexpr int TerrainSlotCount = static_cast<int>(sizeof(TerrainSlots) / sizeof(TerrainSlots[0]));
        constexpr uint32_t TerrainRowStride = 32u;

        // UI2D —— 界面/文本图集精灵（无光照）
        // 槽数: 1, 行跨度(stride): 16 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot UI2DSlots[] = {
            { "sprite", MaterialDomainTextureRole::Sprite, true, SamplerType::Sampler2DArray, false, 0 },
        };
        constexpr int UI2DSlotCount = static_cast<int>(sizeof(UI2DSlots) / sizeof(UI2DSlots[0]));
        constexpr uint32_t UI2DRowStride = 16u;

        // HumanSkin —— 人体皮肤（预积分 SSS / 次表面散射）
        // 槽数: 4, 行跨度(stride): 32 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot HumanSkinSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true, SamplerType::Sampler2DArray, false, 0 },
            { "normal", MaterialDomainTextureRole::Normal, true, SamplerType::Sampler2DArray, false, 2 },
            { "roughness", MaterialDomainTextureRole::Roughness, true, SamplerType::Sampler2DArray, false, 0 },
            { "curvature", MaterialDomainTextureRole::Curvature, true, SamplerType::Sampler2DArray, false, 0 },
        };
        constexpr int HumanSkinSlotCount = static_cast<int>(sizeof(HumanSkinSlots) / sizeof(HumanSkinSlots[0]));
        constexpr uint32_t HumanSkinRowStride = 32u;

        // UnlitTexture —— 无光单图（2D NDC 贴面）
        // 槽数: 1, 行跨度(stride): 16 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot UnlitTextureSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true, SamplerType::Sampler2DArray, true, 0 },
        };
        constexpr int UnlitTextureSlotCount = static_cast<int>(sizeof(UnlitTextureSlots) / sizeof(UnlitTextureSlots[0]));
        constexpr uint32_t UnlitTextureRowStride = 16u;

        // Text2D —— GPU 文本（SDF / 位图两态共用一张字体图集）
        // 槽数: 1, 行跨度(stride): 16 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot Text2DSlots[] = {
            { "base_color", MaterialDomainTextureRole::BaseColor, true, SamplerType::Sampler2DArray, true, 0 },
        };
        constexpr int Text2DSlotCount = static_cast<int>(sizeof(Text2DSlots) / sizeof(Text2DSlots[0]));
        constexpr uint32_t Text2DRowStride = 16u;

        // Sky —— 天空环境（可选一张 cube 贴图）
        // 槽数: 1, 行跨度(stride): 16 字节 (16字节对齐)
        constexpr MaterialDomainTextureSlot SkySlots[] = {
            { "sky_cube", MaterialDomainTextureRole::BaseColor, false, SamplerType::SamplerCube, true, 0 },
        };
        constexpr int SkySlotCount = static_cast<int>(sizeof(SkySlots) / sizeof(SkySlots[0]));
        constexpr uint32_t SkyRowStride = 16u;

        // UnlitTexture3D —— 复用 UnlitTexture 的纹理槽
        constexpr const MaterialDomainTextureSlot *UnlitTexture3DSlots = UnlitTextureSlots;
        constexpr int UnlitTexture3DSlotCount = UnlitTextureSlotCount;
        constexpr uint32_t UnlitTexture3DRowStride = UnlitTextureRowStride;

        struct DomainSlotInfo
        {
            const MaterialDomainTextureSlot *slots;
            int count;
            uint32_t row_stride;
        };

        constexpr DomainSlotInfo GetDomainSlotInfo(MaterialDomain domain) noexcept
        {
            switch (domain)
            {
            case MaterialDomain::StandardOpaque:
                return { StandardOpaqueSlots, StandardOpaqueSlotCount, StandardOpaqueRowStride };
            case MaterialDomain::Foliage:
                return { FoliageSlots, FoliageSlotCount, FoliageRowStride };
            case MaterialDomain::Terrain:
                return { TerrainSlots, TerrainSlotCount, TerrainRowStride };
            case MaterialDomain::UI2D:
                return { UI2DSlots, UI2DSlotCount, UI2DRowStride };
            case MaterialDomain::HumanSkin:
                return { HumanSkinSlots, HumanSkinSlotCount, HumanSkinRowStride };
            case MaterialDomain::UnlitTexture:
                return { UnlitTextureSlots, UnlitTextureSlotCount, UnlitTextureRowStride };
            case MaterialDomain::UnlitTexture3D:
                return { UnlitTexture3DSlots, UnlitTexture3DSlotCount, UnlitTexture3DRowStride };
            case MaterialDomain::Text2D:
                return { Text2DSlots, Text2DSlotCount, Text2DRowStride };
            case MaterialDomain::Sky:
                return { SkySlots, SkySlotCount, SkyRowStride };
            default:
                return { nullptr, 0, 0u };
            }
        }

    } // namespace generated
} // namespace hgl::graph::mtl
