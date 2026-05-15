#include <hgl/mtl/SurfaceFragmentTemplate.h>

namespace hgl::graph::mtl
{
    const SurfaceFragmentTemplate kSurfaceFragmentTemplates[] = {
        { "FS_PureColor", MaterialPreset::PureColor3D, SurfaceType::Unlit, SurfaceShadingModel::PureColor,
          LightingModel::Lambert, 0u, 0u, 0u, "surface/purecolor3d_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::PureColor3D },

        { "FS_VertexColor", MaterialPreset::VertexColor3D, SurfaceType::Unlit, SurfaceShadingModel::VertexColor,
          LightingModel::Lambert, 0u, 0u, 0u, "surface/unlit_vertexcolor_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::VertexColor3D },

        { "FS_UnlitTexture3D", MaterialPreset::UnlitTexture3D, SurfaceType::Unlit, SurfaceShadingModel::UnlitTexture3D,
          LightingModel::Lambert, 1u << static_cast<uint32>(SamplerSlot::BaseColor), 0u, 0u, "surface/unlit_texture3d_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::UnlitTexture3D },

        { "FS_BillboardTexture", MaterialPreset::Billboard2DDynamic, SurfaceType::Unlit, SurfaceShadingModel::BillboardTexture,
          LightingModel::Lambert, 1u << static_cast<uint32>(SamplerSlot::BaseColor), 0u, 0u, "surface/billboard_texture_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::BillboardDynamic },

        { "FS_Text2D", MaterialPreset::Text2D, SurfaceType::Text2D, SurfaceShadingModel::Text,
          LightingModel::Lambert, 1u << static_cast<uint32>(SamplerSlot::BaseColor), 0u, 0u, "", "2d/text2d.frag.glsl", {}, {}, StaticMaterialDefIdHint::Text2D },

        { "FS_StandardLambert", MaterialPreset::Standard,
          SurfaceType::Standard, SurfaceShadingModel::StandardLambert,
          LightingModel::Lambert,
          1u << static_cast<uint32>(SamplerSlot::BaseColor),
          1u << static_cast<uint32>(SamplerSlot::Normal),
          0u,
          "surface/standard_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::Standard3D },

        { "FS_ErrorIndicator", MaterialPreset::PureColor3D, SurfaceType::Unlit, SurfaceShadingModel::CheckerboardFallback,
          LightingModel::Lambert, 0u, 0u, 0u, "surface/error_indicator_surface.glsl", "", {}, {}, StaticMaterialDefIdHint::None },
    };

    const size_t kSurfaceFragmentTemplatesCount = std::size(kSurfaceFragmentTemplates);
}
