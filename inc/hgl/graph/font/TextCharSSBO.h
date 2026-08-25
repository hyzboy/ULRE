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

    // Per-style data (32 bytes, std430) -> GPU SSBO binding 15
    struct CharStyleGPU
    {
        uint32_t text_color;        // offset  0: packed RGBA8 字符颜色
        uint32_t outline_color;     // offset  4: packed RGBA8 勾边颜色
        uint32_t shadow_color;      // offset  8: packed RGBA8 阴影颜色，默认黑
        uint32_t flags;             // offset 12: bit0 = shadow_enabled
        float    italic;            // offset 16: shear angle in radians (0=upright, >0=right-leaning)
        float    bold_px;           // offset 20: 加粗宽度(像素)，0=关闭
        float    outline_px;        // offset 24: 勾边宽度(像素)，0=关闭，钳制 <= TEXT_SDF_SPREAD
        uint32_t shadow_uv_offset;  // offset 28: packed half2 (du, dv)，阴影 UV 偏移(图集归一化坐标)
    };  // 32 bytes

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
