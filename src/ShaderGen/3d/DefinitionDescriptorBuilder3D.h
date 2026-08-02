#pragma once

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <vector>

namespace hgl::graph::mtl
{

struct Build3DDescriptorOptions
{
    uint32_t sky_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
    uint32_t color_palette_stage_flags = uint32_t(VK_SHADER_STAGE_VERTEX_BIT);
    uint32_t texture_stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
    uint32_t material_texture_layer_table_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
};

inline const char *GetTextureNameBySlot(const TextureSlot slot) noexcept
{
    switch (slot)
    {
    case TextureSlot::BaseColor: return "TextureBaseColor";
    case TextureSlot::Normal: return "TextureNormal";
    case TextureSlot::Roughness: return "TextureRoughness";
    default: return "TextureCustom";
    }
}

inline std::vector<FixedDescriptorEntry> Build3DDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    const Build3DDescriptorOptions &opt = {})
{
    std::vector<FixedDescriptorEntry> descriptors;
    descriptors.reserve(16);

    for (const auto semantic : definition.ubo_requirements)
    {
        switch (semantic)
        {
        case UBODescriptorSemantic::ViewportInfo:
            descriptors.push_back({
                DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo,
                TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
            });
            break;
        case UBODescriptorSemantic::CameraInfo:
            descriptors.push_back({
                DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo,
                TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
            });
            break;
        case UBODescriptorSemantic::SkyInfo:
            descriptors.push_back({
                DescriptorSetType::Scene, DescriptorKind::UBO, opt.sky_stage_flags,
                "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo,
                TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
            });
            break;
        case UBODescriptorSemantic::MaterialColorPalette:
            descriptors.push_back({
                DescriptorSetType::Material, DescriptorKind::UBO, opt.color_palette_stage_flags,
                "color_pattle", "ColorPattle", nullptr, DescriptorSemantic::MaterialColorPalette,
                TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
            });
            break;
        }
    }

    if (definition.with_local_to_world)
    {
        descriptors.push_back({
            DescriptorSetType::Transform, TransformDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld,
            TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined,
            GetDescriptorSemanticLayerByKind(TransformDescriptorKind)
        });
        descriptors.push_back({
            DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable,
            TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
        });
    }

    if (!definition.ssbo_slot_decls.empty())
    {
        descriptors.push_back({
            DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialSSBOSlotData,
            TextureSlot::BaseColor, DefaultMaterialSSBOSlot, definition.ssbo_slot_decls.front().ssbo_type,
            GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind)
        });
        descriptors.push_back({
            DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable,
            TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
        });
        descriptors.push_back({
            DescriptorSetType::Material, DescriptorKind::SSBO, opt.material_texture_layer_table_stage_flags,
            "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable,
            TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
        });
    }

    for (const auto &tex : definition.texture_slot_decls)
    {
        descriptors.push_back({
            DescriptorSetType::Material, DescriptorKind::Texture, opt.texture_stage_flags,
            GetTextureNameBySlot(tex.slot), nullptr, ToGLSLSamplerTypeName(tex.sampler_type), DescriptorSemantic::MaterialTexture,
            tex.slot, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Texture
        });
    }

    return descriptors;
}

} // namespace hgl::graph::mtl
