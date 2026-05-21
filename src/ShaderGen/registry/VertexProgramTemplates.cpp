#include <hgl/mtl/VertexProgramTemplate.h>

namespace hgl::graph::mtl
{
    // Resource contracts for common 3D mesh variants: VS always needs camera + transform.
    static constexpr MaterialResourceRequirements kMesh3DVertexContract = {
        .needs_camera    = true,
        .needs_transform = true,
    };

    const VertexProgramTemplate kVertexProgramTemplates[] = {
        { "VS_PureColor3D", MaterialPreset::PureColor,
          GeometryMode::Mesh3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::VAB_Vec3, 0xFFFFFFFFu, "", {}, kMesh3DVertexContract, StaticMaterialDefIdHint::PureColor3D },

        { "VS_VertexColor3D", MaterialPreset::VertexColor,
          GeometryMode::Mesh3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::VAB_Vec3, 0xFFFFFFFFu, "", {}, kMesh3DVertexContract, StaticMaterialDefIdHint::VertexColor3D },

        { "VS_UnlitTexture3D", MaterialPreset::UnlitTexture,
          GeometryMode::Mesh3D, VertexTransformPolicy::Mesh3D,
          PositionProviderId::VAB_Vec3, 0xFFFFFFFFu, "", {}, kMesh3DVertexContract, StaticMaterialDefIdHint::UnlitTexture3D },

        // Billboard variants now use the generic two-axis VS (position_provider/vab_vec2 +
        // vertex_policy/billboard_*.glsl) — no bespoke template path needed.

        { "VS_Text2D", MaterialPreset::Text2D,
          GeometryMode::Quad2D, VertexTransformPolicy::Text2D,
          PositionProviderId::VAB_Vec3, 0xFFFFFFFFu, "2d/text2d.vert.glsl", {}, {}, StaticMaterialDefIdHint::Text2D },

        { "VS_FullscreenTriangle", MaterialPreset::FullscreenTriangle,
          GeometryMode::Mesh3D, VertexTransformPolicy::FullscreenTriangle,
          PositionProviderId::PCG_FullscreenTriangle, 0xFFFFFFFFu, "", {}, {}, StaticMaterialDefIdHint::FullscreenTriangle },
    };

    const size_t kVertexProgramTemplatesCount = std::size(kVertexProgramTemplates);
}
