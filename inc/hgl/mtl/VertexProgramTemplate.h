#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/PositionProvider.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <string_view>

namespace hgl::graph::mtl
{
    struct VertexProgramTemplate
    {
        const char *name = "";
        MaterialPreset preset = MaterialPreset::PureColor3D;
        GeometryMode geometry_mode = GeometryMode::Mesh3D;
        VertexInputProfile vertex_input = VertexInputProfile::Unknown;
        VertexTransformPolicy vertex_policy = VertexTransformPolicy::Unknown;
        PositionProviderId position_provider = PositionProviderId::DirectVec3;
        uint32 supported_va_bits_mask = 0;
        const char *vs_template_path = "";
        ShaderStageFeatureDesc vs_features{};
        MaterialResourceRequirements resource_contract{};
        StaticMaterialDefIdHint def_hint = StaticMaterialDefIdHint::None;
    };
}
