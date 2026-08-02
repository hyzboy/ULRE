#pragma once

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/common/RenderOptions.h>
#include <vector>

namespace hgl::graph::mtl::descriptor_builder_common
{

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

inline void PushViewport(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushCamera(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "camera", "CameraInfo", nullptr, DescriptorSemantic::CameraInfo,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushSky(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Scene, DescriptorKind::UBO, stage_flags,
        "sky", "SkyInfo", nullptr, DescriptorSemantic::SkyInfo,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO
    });
}

inline void PushLocalToWorld(std::vector<FixedDescriptorEntry> &v, const DescriptorKind kind, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Transform, kind, stage_flags,
        "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(kind)
    });
}

inline void PushLocalToWorldIndexRows(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Transform, DescriptorKind::SSBO, stage_flags,
        "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialInstance(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags, const SSBOType ssbo_type)
{
    v.push_back({
        DescriptorSetType::Material, MaterialInstanceDescriptorKind, stage_flags,
        "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialSSBOSlotData,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, ssbo_type, GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind)
    });
}

inline void PushMaterialDataIndexRows(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialTextureLayerRows(std::vector<FixedDescriptorEntry> &v, const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::SSBO, stage_flags,
        "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable,
        TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO
    });
}

inline void PushMaterialTexture(std::vector<FixedDescriptorEntry> &v,
                                const TextureSlot slot,
                                const char *glsl_type,
                                const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::Texture, stage_flags,
        GetTextureNameBySlot(slot), nullptr, glsl_type, DescriptorSemantic::MaterialTexture,
        slot, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Texture
    });
}

inline void PushMaterialSampler(std::vector<FixedDescriptorEntry> &v,
                                const char *name,
                                const TextureSlot slot,
                                const char *glsl_type,
                                const uint32_t stage_flags)
{
    v.push_back({
        DescriptorSetType::Material, DescriptorKind::TextureSampler, stage_flags,
        name, nullptr, glsl_type, DescriptorSemantic::MaterialSampler,
        slot, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler
    });
}

} // namespace hgl::graph::mtl::descriptor_builder_common
