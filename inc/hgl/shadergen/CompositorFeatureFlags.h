#pragma once

#include <hgl/common/PositionProvider.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/CoordinateSystem.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <hgl/shadergen/FragmentProviderRegistry.h>
#include <cstddef>
#include <string>

namespace hgl::graph {

struct CompositorFeatureFlags
{
    // Vertex stage flags
    PositionProviderId            position_provider = PositionProviderId::DirectVec3;
    mtl::VertexTransformPolicy    vertex_policy     = mtl::VertexTransformPolicy::Mesh3D;
    uint32 vertex_attrib_bits = 0;
    bool has_direction    = false;

    bool HasVertexAttrib(const VertexAttrib attrib)const
    {
        if(attrib < VertexAttrib::Position || attrib >= VertexAttrib::RANGE_SIZE)
            return false;

        return (vertex_attrib_bits & (1u << static_cast<uint32>(attrib))) != 0;
    }

    void SetVertexAttrib(const VertexAttrib attrib, const bool enabled = true)
    {
        if(attrib < VertexAttrib::Position || attrib >= VertexAttrib::RANGE_SIZE)
            return;

        const uint32 bit = 1u << static_cast<uint32>(attrib);
        if(enabled)
            vertex_attrib_bits |= bit;
        else
            vertex_attrib_bits &= ~bit;
    }

    // Fragment stage flags
    FragmentProviderId            fragment_provider = FragmentProviderId::Default;
    bool enable_lighting  = false;
    bool needs_camera     = false;
    bool needs_transform  = false;
    bool alpha_masked     = false;
    bool alpha_dither     = false;
    bool has_texcoord     = false;
    bool has_clip_pos     = false;

    // Sky ambient / lighting model: only active when enable_lighting == true.
    // Set from MaterialVariantKey (ECS-injected LOD decision); not stored per-preset.
    mtl::SkyLightAmbientModel sky_ambient_model = mtl::SkyLightAmbientModel::Simple;
    mtl::LightingModel lighting_model = mtl::LightingModel::Lambert;

    // Surface function path (FS only)
    std::string surface_path;

    // 2D coordinate system (only used when position_provider == VAB_Vec2).
    // NDC = no extra transform; Ortho / ZeroToOne emit corresponding GLSL macros.
    CoordinateSystem2D coord_2d = CoordinateSystem2D::NDC;
};

} // namespace hgl::graph
