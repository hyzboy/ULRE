#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>

namespace hgl::graph::mtl
{
    // VertexVaryingConfig — declares what varying outputs the generated vertex
    // stage (mesh shader, after VS deprecation) emits.
    // These must match the FS template inputs.
    struct VertexVaryingConfig
    {
        bool emit_data_index_id    = false;  // location=0 flat out uint fragDataIndexID
        bool emit_vertex_color     = false;  // location=2 out vec4 fragVertexColor
        bool emit_uv0              = false;  // location=3 out vec2 fragUV0
        // Extended: for materials that need world-space outputs (e.g. DebugNormalColor)
        // These are appended after the standard varying slots above.
        bool emit_world_pos        = false;  // out vec3 fragWorldPos
        bool emit_world_normal     = false;  // out vec3 fragWorldNormal
        // Extended: special-purpose varying outputs (appended last).
        bool emit_luminance        = false;  // out float fragLuminance (= Luminance vertex attr)
        bool emit_frag_direction   = false;  // out vec3 fragDirection (= normalize(Position)), sky dome
        // Palette-color materials: GetL2W reads the TransformID vertex attribute
        // directly (l2w.mats[TransformID]) instead of resolving gl_InstanceIndex
        // through the l2w_index_rows table.
        bool use_transform_id_attr = false;
        // Vertex color sourced from a MaterialColorPalette UBO instead of a Color
        // vertex attribute: fragVertexColor = color_palette.color[ColorIndex].
        bool emit_vertex_color_from_palette = false;
        // Text CharQuad: flat per-vertex style index into sbo_char_style
        // (location=4 flat out uint fragStyleID).
        bool emit_style_id           = false;
    };
}
