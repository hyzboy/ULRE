#include <hgl/mtl/VertexProgramTemplate.h>

namespace hgl::graph::mtl
{
    const VertexProgramTemplate kVertexProgramTemplates[] = {
        { "VS_PureColor3D", MaterialPreset::PureColor3D,
          GeometryMode::Mesh3D, VertexInputProfile::Position3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "", {}, {}, StaticMaterialDefIdHint::PureColor3D },

        { "VS_VertexColor3D", MaterialPreset::VertexColor3D,
          GeometryMode::Mesh3D, VertexInputProfile::PositionColor3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "", {}, {}, StaticMaterialDefIdHint::VertexColor3D },

        { "VS_UnlitTexture3D", MaterialPreset::UnlitTexture3D,
          GeometryMode::Mesh3D, VertexInputProfile::PositionTexCoord3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "", {}, {}, StaticMaterialDefIdHint::UnlitTexture3D },

        { "VS_BillboardDynamic", MaterialPreset::Billboard2DDynamic,
          GeometryMode::BillboardCameraFacing, VertexInputProfile::BillboardPositionOnly3D, VertexTransformPolicy::BillboardCameraFacing,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "compositor/main_forward_billboard_dynamic.vert.glsl", {}, {}, StaticMaterialDefIdHint::BillboardDynamic },

        { "VS_BillboardFixed", MaterialPreset::Billboard2DFixed,
          GeometryMode::BillboardAxisLocked, VertexInputProfile::BillboardPositionOnly3D, VertexTransformPolicy::BillboardAxisLocked,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "compositor/main_forward_billboard_fixed.vert.glsl", {}, {}, StaticMaterialDefIdHint::BillboardFixed },

        { "VS_Text2D", MaterialPreset::Text2D,
          GeometryMode::Quad2D, VertexInputProfile::PositionTexCoord2D, VertexTransformPolicy::Text2D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "2d/text2d.vert.glsl", {}, {}, StaticMaterialDefIdHint::Text2D },

        { "VS_FullscreenTriangle", MaterialPreset::FullscreenTriangle,
          GeometryMode::Mesh3D, VertexInputProfile::FullscreenProcedural, VertexTransformPolicy::FullscreenTriangle,
          PositionProviderId::PCG_FullscreenTriangle, 0xFFFFFFFFu, "", {}, {}, StaticMaterialDefIdHint::FullscreenTriangle },
    };

    const size_t kVertexProgramTemplatesCount = std::size(kVertexProgramTemplates);
}
