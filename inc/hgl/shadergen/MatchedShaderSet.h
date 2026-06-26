#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/RenderPhase.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <hgl/common/PositionProvider.h>

namespace hgl::graph::mtl
{
    struct MatchedShaderSet
    {
        bool matched = false;
        bool used_fallback = false;

        MaterialPreset preset = MaterialPreset::Checkerboard3D;
        MaterialLOD    quality_level = MaterialLOD::Base;
        RenderPhase    render_phase = RenderPhase::Forward;

        const char *surface_path = nullptr;

        // Snapshot axes for diagnostics / bridge layers.
        LightingModel        lighting_model = LightingModel::Lambert;
        SkyLightAmbientModel sky_ambient_model = SkyLightAmbientModel::Simple;
        VertexTransformPolicy policy = VertexTransformPolicy::Unknown;
        PositionProviderId   position_provider = PositionProviderId::DirectVec3;

        // Optional routing overrides.
        bool     has_pass_override = false;
        PassType pass_override = PassType::ForwardOpaque;

        const char *failure_reason = nullptr;

        bool IsValid() const noexcept
        {
            return matched && surface_path && surface_path[0];
        }
    };
}
