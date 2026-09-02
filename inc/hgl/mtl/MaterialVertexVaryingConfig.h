#pragma once

namespace hgl::graph::mtl
{
    struct MaterialVertexVaryingConfig
    {
        bool emit_data_index_id = false;
        bool emit_vertex_color = false;
        bool emit_uv0 = false;
        bool emit_world_pos = false;
        bool emit_world_normal = false;
        bool emit_luminance = false;
        bool emit_frag_direction = false;
        bool use_transform_id_attr = false;
        bool emit_vertex_color_from_palette = false;
        bool emit_style_id = false;
    };
}
