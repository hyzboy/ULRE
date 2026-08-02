#pragma once

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/graph/ssbo/MaterialInstanceLayout.h>
#include <vector>
#include "../common/DescriptorBuilderCommon.h"

namespace hgl::graph::mtl
{

struct Build3DDescriptorOptions
{
    uint32_t sky_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
    uint32_t color_palette_stage_flags = uint32_t(VK_SHADER_STAGE_VERTEX_BIT);
    uint32_t texture_stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
    uint32_t material_texture_layer_table_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
};

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
            descriptor_builder_common::PushViewport(descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
            break;
        case UBODescriptorSemantic::CameraInfo:
            descriptor_builder_common::PushCamera(descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
            break;
        case UBODescriptorSemantic::SkyInfo:
            descriptor_builder_common::PushSky(descriptors, opt.sky_stage_flags);
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

    if (definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
     && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough)
    {
        descriptor_builder_common::PushLocalToWorld(descriptors, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        descriptor_builder_common::PushLocalToWorldIndexRows(descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
    }

    if (!definition.ssbo_slot_decls.empty())
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(definition.ssbo_slot_decls.size()); ++i)
        {
            const auto &decl = definition.ssbo_slot_decls[i];
            descriptor_builder_common::PushMaterialInstance(
                descriptors,
                uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                decl.name.c_str(),
                ssbo::GetMaterialSSBOStructName(decl.ssbo_type),
                i,
                decl.ssbo_type);
        }
        descriptor_builder_common::PushMaterialDataIndexRows(descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        descriptor_builder_common::PushMaterialTextureLayerRows(descriptors, opt.material_texture_layer_table_stage_flags);
    }

    for (const auto &tex : definition.texture_slot_decls)
    {
        descriptor_builder_common::PushMaterialTexture(
            descriptors,
            tex.slot,
            ToGLSLSamplerTypeName(tex.sampler_type),
            opt.texture_stage_flags);
    }

    return descriptors;
}

} // namespace hgl::graph::mtl
