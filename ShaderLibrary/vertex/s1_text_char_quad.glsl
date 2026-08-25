// @ulre begin
// @ulre name s1_text_char_quad
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// 注：三个文本数据 SSBO（TextCharInfo b14 / CharStyleData b15 / CharInstanceData b16）
//     为 CharQuad 专用，绑定由 MaterialShaderCompiler 硬编码注册，不走 @ulre ssbo 元数据。
// Stage 1: 文本字符 Quad SSBO 声明——三层数据模型（Mesh Shader TextCharQuad 模式）
//
// 三层结构：
//   1. TextCharInfo  —— 每唯一字符度量 + UV（16B/条目）
//      metrics_x/y: int16  packed in int32（符号扩展低 16 位）
//      metrics_w/h: uint16 packed in uint32
//      uv_*:        half-float (uint16 bit pattern stored in uint32)
//   2. CharStyleData —— 每唯一样式（8B/条目）
//      text_color:  packed RGBA8 (uint32)
//      italic:      shear angle in radians (float)
//   3. CharInstanceData —— 每字符实例（8B/条目）
//      pen_x/y:     int16  packed in int32
//      char_id:     uint16 packed in uint32
//      style_id:    uint16 packed in uint32
//
// std430 对齐：TextCharInfo=16B, CharStyleData=36B, CharInstanceData=8B
// 所有字段自然对齐，无额外 padding
#ifndef S1_TEXT_CHAR_QUAD_GLSL
#define S1_TEXT_CHAR_QUAD_GLSL

// ── Per-unique-char 度量 + UV（16 bytes per entry）──
struct TextCharInfo {
    int   offset_xy;    // int16_x (low16) + int16_y (high16) packed in int32
    uint  metrics_wh;   // uint16_w (low16) + uint16_h (high16) packed in uint32
    uint  uv_lt;        // half_left (low16) + half_top (high16) packed in uint32
    uint  uv_rb;        // half_right (low16) + half_bottom (high16) packed in uint32
};

layout(set=PER_OBJECT_SET, binding=TEXT_CHARINFO_BINDING, std430) readonly buffer TextCharInfoData {
    TextCharInfo chars[];
} sbo_char_info;

// ── Per-style 数据（36 bytes per entry）──
struct CharStyleData {
    uint  text_color;       // packed RGBA8
    uint  outline_color;    // packed RGBA8
    uint  shadow_color;     // packed RGBA8
    uint  flags;            // bit0 = shadow_enabled
    float italic;           // shear angle in radians
    float bold_px;          // bold width in pixels
    float outline_px;       // outline width in pixels
    uint  shadow_uv_offset; // packHalf2x16 UV offset
    float scale;            // scale factor
};

layout(set=PER_OBJECT_SET, binding=TEXT_CHARSTYLE_BINDING, std430) readonly buffer CharStyleDataBuf {
    CharStyleData styles[];
} sbo_char_style;

// ── Per-char-instance 数据（8 bytes per entry）──
struct CharInstanceData {
    int   pen_xy;       // int16_x (low16) + int16_y (high16) packed in int32
    uint  char_style;   // uint16_char_id (low16) + uint16_style_id (high16) packed in uint32
};

layout(set=PER_OBJECT_SET, binding=TEXT_CHARINSTANCE_BINDING, std430) readonly buffer CharInstanceDataBuf {
    CharInstanceData instances[];
} sbo_char_instance;

#endif // S1_TEXT_CHAR_QUAD_GLSL
