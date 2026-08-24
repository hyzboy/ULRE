#ifndef HGL_GRAPH_FONT_TEXT_CHAR_SSBO_H
#define HGL_GRAPH_FONT_TEXT_CHAR_SSBO_H

#include <cstdint>

namespace hgl { namespace graph { namespace layout {

    // Per-unique-char info (16 bytes, std430) -> GPU SSBO binding 14
    struct TextCharInfo
    {
        int16_t  offset_x;    // CharMetricsInfo.x - glyph display offset
        int16_t  offset_y;    // CharMetricsInfo.y
        uint16_t metrics_w;   // CharMetricsInfo.w - glyph pixel width
        uint16_t metrics_h;   // CharMetricsInfo.h - glyph pixel height
        uint16_t uv_left;     // half-float atlas UV
        uint16_t uv_top;
        uint16_t uv_right;
        uint16_t uv_bottom;
    };  // 16 bytes

    // Per-style data (8 bytes, std430) -> GPU SSBO binding 15
    struct CharStyleGPU
    {
        uint32_t text_color;  // packed RGBA8
        float    italic;      // shear angle in radians (0=upright, >0=right-leaning)
    };  // 8 bytes

    // Per-char-instance data (8 bytes, std430) -> GPU SSBO binding 16
    struct CharInstance
    {
        int16_t  pen_x;       // screen X position (from CPU layout)
        int16_t  pen_y;       // screen Y position (from CPU layout)
        uint16_t char_id;     // index into TextCharInfo SSBO
        uint16_t style_id;    // index into CharStyleGPU SSBO
    };  // 8 bytes

}}}// namespace hgl::graph::layout

#endif
