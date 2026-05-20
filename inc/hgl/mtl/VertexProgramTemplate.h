#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/PositionProvider.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <cstddef>
#include <string_view>

namespace hgl::graph::mtl
{
    struct VertexProgramTemplate
    {
        const char *name = "";
        MaterialPreset preset = MaterialPreset::PureColor;
        GeometryMode geometry_mode = GeometryMode::Mesh3D;
        VertexTransformPolicy vertex_policy = VertexTransformPolicy::Unknown;
        PositionProviderId position_provider = PositionProviderId::DirectVec3;
        uint32 supported_va_bits_mask = 0;
        const char *vs_template_path = "";
        ShaderStageFeatureDesc vs_features{};
        MaterialResourceRequirements resource_contract{};
        StaticMaterialDefIdHint def_hint = StaticMaterialDefIdHint::None;
    };

    extern const VertexProgramTemplate kVertexProgramTemplates[];
    extern const size_t                kVertexProgramTemplatesCount;
}
