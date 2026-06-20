#include <hgl/mtl/SurfaceFragmentTemplate.h>

namespace hgl::graph::mtl
{
    // Fragment contract for surfaces that read MaterialBindingInstance data.
    static constexpr MaterialResourceRequirements kMaterialInstanceFSContract = {
        .needs_material_instance = true,
    };

    // Fragment contract for lit surfaces: needs sky + material instance.
    static constexpr MaterialResourceRequirements kLitMaterialInstanceFSContract = {
        .needs_sky               = true,
        .needs_material_instance = true,
        .enable_lighting         = true,
    };

    const SurfaceFragmentTemplate kSurfaceFragmentTemplates[] = {
        { "FS_PureColor", MaterialPreset::PureColor3D, SurfaceType::Unlit, SurfaceShadingModel::PureColor,
          LightingModel::Lambert, 0u, 0u, 0u, "surface/unlit_color3d_surface.glsl", "", {}, kMaterialInstanceFSContract, StaticMaterialDefIdHint::PureColor3D, ShaderDataSchema::Color4f },

        { "FS_VertexColor", MaterialPreset::VertexColor3D, SurfaceType::Unlit, SurfaceShadingModel::VertexColor,
          LightingModel::Lambert, 0u, 0u, 0u, "surface/unlit_vertexcolor_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::VertexColor3D, ShaderDataSchema::Color4f },

        { "FS_UnlitTexture3D", MaterialPreset::UnlitTexture3D, SurfaceType::Unlit, SurfaceShadingModel::UnlitTexture3D,
          LightingModel::Lambert, 1u << static_cast<uint32>(SamplerSlot::BaseColor), 0u, 0u, "surface/unlit_texture3d_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::UnlitTexture3D, ShaderDataSchema::Color4f },

        { "FS_BillboardTexture", MaterialPreset::Billboard2DDynamic, SurfaceType::Unlit, SurfaceShadingModel::BillboardTexture,
          LightingModel::Lambert, 1u << static_cast<uint32>(SamplerSlot::BaseColor), 0u, 0u, "surface/billboard_texture_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::BillboardDynamic, ShaderDataSchema::BillboardSizeUVec2, true },

        { "FS_Text2D", MaterialPreset::Text2D, SurfaceType::Text2D, SurfaceShadingModel::Text,
          LightingModel::Lambert, 1u << static_cast<uint32>(SamplerSlot::BaseColor), 0u, 0u, "", "2d/text2d.frag.glsl", {}, {}, StaticMaterialDefIdHint::Text2D, ShaderDataSchema::TextColor },

        { "FS_StandardLambert", MaterialPreset::Standard,
          SurfaceType::Standard, SurfaceShadingModel::StandardLambert,
          LightingModel::Lambert,
          1u << static_cast<uint32>(SamplerSlot::BaseColor),
          1u << static_cast<uint32>(SamplerSlot::Normal),
          0u,
          "surface/standard_surface.glsl", "", {}, kLitMaterialInstanceFSContract, StaticMaterialDefIdHint::Standard3D, ShaderDataSchema::StandardParams },

        { "FS_ErrorIndicator", MaterialPreset::PureColor3D, SurfaceType::Unlit, SurfaceShadingModel::CheckerboardFallback,
          LightingModel::Lambert, 0u, 0u, 0u, "surface/error_indicator_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::None, ShaderDataSchema::None },
    };

    const size_t kSurfaceFragmentTemplatesCount = std::size(kSurfaceFragmentTemplates);
}
