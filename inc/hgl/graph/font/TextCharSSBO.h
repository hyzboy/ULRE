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

    // Per-style data (40 bytes, std430) -> GPU SSBO binding 15
    // 同时作为 CPU 侧样式定义（CPU→GPU 转换在 TextRenderPipeline 中完成）
    struct CharStyle
    {
        uint32_t text_color;        // offset  0: packed RGBA8 字符颜色
        uint32_t outline_color;     // offset  4: packed RGBA8 勾边颜色
        uint32_t shadow_color;      // offset  8: packed RGBA8 阴影颜色，默认黑
        uint32_t flags;             // offset 12: bit0 = shadow_enabled
        float    italic;            // offset 16: shear angle in radians (0=upright, >0=right-leaning)
        float    bold_px;           // offset 20: 加粗宽度(像素)，0=关闭
        float    outline_px;        // offset 24: 勾边宽度(像素)，0=关闭，钳制 <= TEXT_SDF_SPREAD
        uint32_t shadow_uv_offset;  // offset 28: packed half2 (du, dv)，阴影 UV 偏移(图集归一化坐标)
        float    scale = 1.0f;      // offset 32: 缩放因子，1.0=原始大小
        int32_t  rotation = 0;      // offset 36: 旋转角度 (0=无旋转, 90=右转, 180=翻转, 270=左转)

        bool operator==(const CharStyle& o) const
        {
            return text_color == o.text_color
                && outline_color == o.outline_color
                && shadow_color == o.shadow_color
                && flags == o.flags
                && italic == o.italic
                && bold_px == o.bold_px
                && outline_px == o.outline_px
                && shadow_uv_offset == o.shadow_uv_offset
                && scale == o.scale
                && rotation == o.rotation;
        }
        bool operator!=(const CharStyle& o) const { return !(*this == o); }
    };  // 40 bytes

    // ── 颜色打包：与 GLSL packUnorm4x8 对称 ──────────────────────────
    // GLSL packUnorm4x8(r,g,b,a) = r | g<<8 | b<<16 | a<<24
    //   （最低字节 = r，unpackUnorm4x8 按同样顺序解包，语义唯一、无
    //    rgba8/abgr8 歧义）。
    // HGL_U8_TO_RGBA8 则打包为 r<<24 | g<<16 | b<<8 | a（r 在最高字节），
    // 字节序相反。因此 CharStyle 上传 GPU 前须经 HGL_TO_PACKUNORM4x8
    // 转换，GLSL 端即可直接使用标准 unpackUnorm4x8。
    inline uint32_t PackUnorm4x8(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        return static_cast<uint32_t>(r)
             | (static_cast<uint32_t>(g) << 8)
             | (static_cast<uint32_t>(b) << 16)
             | (static_cast<uint32_t>(a) << 24);
    }

    // HGL_U8_TO_RGBA8（r 最高字节）→ packUnorm4x8 序（r 最低字节）
    inline uint32_t HGL_TO_PACKUNORM4x8(uint32_t rgba8)
    {
        return ((rgba8 >> 24) & 0xFF)
             | (((rgba8 >> 16) & 0xFF) << 8)
             | (((rgba8 >> 8)  & 0xFF) << 16)
             | ((rgba8 & 0xFF) << 24);
    }

    // Per-char-instance data (8 bytes, std430) -> GPU SSBO binding 16
    struct CharInstance
    {
        int16_t  pen_x;       // screen X position (from CPU layout)
        int16_t  pen_y;       // screen Y position (from CPU layout)
        uint16_t char_id;     // index into TextCharInfo SSBO
        uint16_t style_id;    // index into CharStyle SSBO
    };  // 8 bytes

}}}// namespace hgl::graph::layout

#endif
