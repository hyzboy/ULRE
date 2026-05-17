#include <hgl/mtl/VertexProgramTemplate.h>

namespace hgl::graph::mtl
{
    // Resource contracts for common 3D mesh variants: VS always needs camera + transform.
    static constexpr MaterialResourceRequirements kMesh3DVertexContract = {
        .needs_camera    = true,
        .needs_sky       = false,
        .needs_transform = true,
    };

    // Billboard VS also needs camera + transform, but no sky.
    static constexpr MaterialResourceRequirements kBillboardVertexContract = {
        .needs_camera    = true,
        .needs_sky       = false,
        .needs_transform = true,
    };

    const VertexProgramTemplate kVertexProgramTemplates[] = {
        { "VS_PureColor3D", MaterialPreset::PureColor3D,
          GeometryMode::Mesh3D, VertexInputProfile::Position3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "", {}, kMesh3DVertexContract, StaticMaterialDefIdHint::PureColor3D },

        { "VS_VertexColor3D", MaterialPreset::VertexColor3D,
          GeometryMode::Mesh3D, VertexInputProfile::PositionColor3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "", {}, kMesh3DVertexContract, StaticMaterialDefIdHint::VertexColor3D },

        { "VS_UnlitTexture3D", MaterialPreset::UnlitTexture3D,
          GeometryMode::Mesh3D, VertexInputProfile::PositionTexCoord3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "", {}, kMesh3DVertexContract, StaticMaterialDefIdHint::UnlitTexture3D },

        { "VS_UnlitTexture3DBillboardDynamic", MaterialPreset::UnlitTexture3D,
          GeometryMode::BillboardCameraFacing, VertexInputProfile::PositionTexCoord2D, VertexTransformPolicy::BillboardCameraFacing,
          PositionProviderId::VAB_Vec2, 0xFFFFFFFFu, "compositor/main_forward_billboard_dynamic.vert.glsl", {}, kBillboardVertexContract, StaticMaterialDefIdHint::UnlitTexture3D },

        { "VS_UnlitTexture3DBillboardFixed", MaterialPreset::UnlitTexture3D,
          GeometryMode::BillboardAxisLocked, VertexInputProfile::PositionTexCoord2D, VertexTransformPolicy::BillboardAxisLocked,
          PositionProviderId::VAB_Vec2, 0xFFFFFFFFu, "compositor/main_forward_billboard_fixed.vert.glsl", {}, kBillboardVertexContract, StaticMaterialDefIdHint::UnlitTexture3D },

        { "VS_Text2D", MaterialPreset::Text2D,
          GeometryMode::Quad2D, VertexInputProfile::PositionTexCoord2D, VertexTransformPolicy::Text2D,
          PositionProviderId::DirectVec3, 0xFFFFFFFFu, "2d/text2d.vert.glsl", {}, {}, StaticMaterialDefIdHint::Text2D },

        { "VS_FullscreenTriangle", MaterialPreset::FullscreenTriangle,
          GeometryMode::Mesh3D, VertexInputProfile::FullscreenProcedural, VertexTransformPolicy::FullscreenTriangle,
          PositionProviderId::PCG_FullscreenTriangle, 0xFFFFFFFFu, "", {}, {}, StaticMaterialDefIdHint::FullscreenTriangle },
    };

    const size_t kVertexProgramTemplatesCount = std::size(kVertexProgramTemplates);
}
