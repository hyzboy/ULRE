#pragma once

// StandardSharedSpec — 共享 descriptor spec（Standard / StandardTextureArray 双形态公共部分）。
//
// 两种形态的 contract shape 等价（已由 BindingContractRegressionGate D case 验证）：
//   - UBO/SSBO descriptor 定义完全一致
//   - MaterialTexture 语义/slot 完全一致
//   - 唯一受控差异：glsl_type（"sampler2D" vs "sampler2DArray"），以及 surface shader 文件
//
// 调用方通过 BuildStandardDescriptors(tex_glsl_type) 获取完整 descriptor 列表。
// SkyLight 注入仍在各自工厂函数中按已有模式追加，不在本头中处理。

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace hgl::graph::mtl
{

// UBO/SSBO 公共部分（与 glsl_type 无关）
constexpr FixedDescriptorEntry STANDARD_SHARED_BASE_DESCRIPTORS[] = {
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo },
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",   nullptr, DescriptorSemantic::CameraInfo },
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky",      "SkyInfo",      nullptr, DescriptorSemantic::SkyInfo },
    { DescriptorSetType::Transform, TransformDescriptorKind,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w",             "LocalToWorldData",    nullptr, DescriptorSemantic::LocalToWorld },
    { DescriptorSetType::Transform, DescriptorKind::SSBO,     uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows",  "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable },
    { DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::PBRSurface },
    { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows",    "DataIndexRows",   nullptr, DescriptorSemantic::MaterialDataIndexTable },
    { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable },
};

constexpr uint32_t STANDARD_SHARED_BASE_COUNT =
    uint32_t(sizeof(STANDARD_SHARED_BASE_DESCRIPTORS) / sizeof(STANDARD_SHARED_BASE_DESCRIPTORS[0]));

// 纹理槽位（语义/slot 固定，glsl_type 由调用方传入区分 2D vs 2DArray）
struct StandardTextureSlotDef
{
    DescriptorSemantic semantic;
    TextureSlot        texture_slot;
    const char *       name;
};

constexpr StandardTextureSlotDef STANDARD_TEXTURE_SLOTS[] = {
    { DescriptorSemantic::MaterialTexture, TextureSlot::BaseColor, "TextureBaseColor" },
    { DescriptorSemantic::MaterialTexture, TextureSlot::Normal,    "TextureNormal"    },
    { DescriptorSemantic::MaterialTexture, TextureSlot::Roughness, "TextureRoughness" },
};

constexpr uint32_t STANDARD_TEXTURE_SLOT_COUNT =
    uint32_t(sizeof(STANDARD_TEXTURE_SLOTS) / sizeof(STANDARD_TEXTURE_SLOTS[0]));

// 构建完整 descriptor 列表：base + texture entries（glsl_type 由调用方指定）
// Actual sampling is bindless; these entries are kept for Recipe/contract extraction only.
inline std::vector<FixedDescriptorEntry> BuildStandardDescriptors(const char *tex_glsl_type)
{
    std::vector<FixedDescriptorEntry> entries(
        STANDARD_SHARED_BASE_DESCRIPTORS,
        STANDARD_SHARED_BASE_DESCRIPTORS + STANDARD_SHARED_BASE_COUNT);

    for (uint32_t i = 0; i < STANDARD_TEXTURE_SLOT_COUNT; ++i)
    {
        const auto &slot = STANDARD_TEXTURE_SLOTS[i];
        entries.push_back({
            DescriptorSetType::Material,
            DescriptorKind::Texture,
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
            slot.name,
            nullptr,
            tex_glsl_type,
            slot.semantic,
            slot.texture_slot,
        });
    }

    return entries;
}

} // namespace hgl::graph::mtl
