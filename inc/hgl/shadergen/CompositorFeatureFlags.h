#pragma once

#include <string>

namespace hgl::graph {

struct CompositorFeatureFlags
{
    // Vertex stage flags
    bool vert_input_2d    = false;
    bool has_uv0          = false;
    bool has_vertex_color = false;
    bool has_world_pos    = false;
    bool has_world_normal = false;
    bool has_luminance    = false;
    bool has_direction    = false;

    // Fragment stage flags
    bool enable_lighting  = false;
    bool needs_camera     = false;
    bool needs_sky        = false;
    bool alpha_masked     = false;
    bool alpha_dither     = false;
    bool has_texcoord     = false;
    bool has_clip_pos     = false;

    // Surface function path (FS only)
    std::string surface_path;
};

} // namespace hgl::graph
