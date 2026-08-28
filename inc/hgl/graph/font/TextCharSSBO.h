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

    // ── 颜色字段约定：packUnorm4x8 序 ─────────────────────────────
    // CharStyle.text_color/outline_color/shadow_color 直接存
    // glm::packUnorm4x8（glm/gtc/packing.hpp）的结果：
    //   packUnorm4x8 = r | g<<8 | b<<16 | a<<24（最低字节 = r）
    // 与 GLSL 标准 unpackUnorm4x8 的字节序一致（语义唯一，无
    // rgba8/abgr8 歧义），GPU 端直接用标准内置函数解包。
    // 注意：HGL_U8_TO_RGBA8 打包为 r<<24 | g<<16 | b<<8 | a
    // （r 在最高字节），与 packUnorm4x8 相反，勿混用。

    // Per-char-instance data (12 bytes, std430) -> GPU SSBO binding 16
    // 注意 alignas(4)：GPU 端 std430 布局为 pen_xy(4)+char_style(4)+rotation(4)=12B，
    // CPU 侧不加对齐 sizeof 只有 10B（全 int16 成员），stride 不匹配会逐实例错位。
    struct alignas(4) CharInstance
    {
        int16_t  pen_x;       // screen X position (from CPU layout)
        int16_t  pen_y;       // screen Y position (from CPU layout)
        uint16_t char_id;     // index into TextCharInfo SSBO
        uint16_t style_id;    // index into CharStyle SSBO
        int16_t  rotation = 0;// 实例级旋转 (0/90/180/270)，与 CharStyle.rotation 叠加；
                              // 竖排 vrotate 字符 = 90
    };  // 12 bytes (alignas(4) 强制，与 GPU std430 一致)

    // ── 布局断言：CPU 结构与 GLSL 真源（ShaderLibrary/vertex/s1_text_char_quad.glsl）
    // 的 std430 布局逐字段对应——改任何一侧的字段顺序/宽度必须同步另一侧，
    // 否则 GPU 端逐实例错位（stride 不匹配）。
    static_assert(sizeof(TextCharInfo) == 16, "TextCharInfo 必须与 GLSL TextCharInfo 一致（4×32位=16B）");
    static_assert(sizeof(CharStyle) == 40, "CharStyle 必须与 GLSL CharStyleData 一致（40B）");
    static_assert(sizeof(CharInstance) == 12, "CharInstance 必须与 GLSL CharInstanceData 一致（12B，alignas(4)）");

}}}// namespace hgl::graph::layout

#endif
