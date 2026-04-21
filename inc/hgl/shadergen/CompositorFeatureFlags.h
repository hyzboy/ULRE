#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <cstddef>
#include <string>

namespace hgl::graph {

struct CompositorFeatureFlags
{
    // Vertex stage flags
    bool vert_input_2d    = false;
    bool vertex_attrib[static_cast<size_t>(VertexAttrib::RANGE_SIZE)] = {};
    bool has_direction    = false;

    bool HasVertexAttrib(const VertexAttrib attrib)const
    {
        if(attrib < VertexAttrib::Position || attrib >= VertexAttrib::RANGE_SIZE)
            return false;

        return vertex_attrib[static_cast<size_t>(attrib)];
    }

    void SetVertexAttrib(const VertexAttrib attrib, const bool enabled = true)
    {
        if(attrib < VertexAttrib::Position || attrib >= VertexAttrib::RANGE_SIZE)
            return;

        vertex_attrib[static_cast<size_t>(attrib)] = enabled;
    }

    // Fragment stage flags
    bool enable_lighting  = false;
    bool needs_camera     = false;
    bool needs_sky        = false;
    bool alpha_masked     = false;
    bool alpha_dither     = false;
    bool has_texcoord     = false;
    bool has_clip_pos     = false;

    // Sky ambient model (only used when needs_sky == true)
    mtl::SkyLightAmbientModel sky_ambient_model = mtl::SkyLightAmbientModel::Simple;

    // Lighting model (only used when enable_lighting == true)
    mtl::LightingModel lighting_model = mtl::LightingModel::Lambert;

    // Surface function path (FS only)
    std::string surface_path;
};

} // namespace hgl::graph
