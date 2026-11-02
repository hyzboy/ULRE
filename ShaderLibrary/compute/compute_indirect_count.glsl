#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 1) in;

// 计数值经 BDA 写入：地址由 push constant 下发，无 set 无 binding
layout(buffer_reference, std430, buffer_reference_align=16) buffer DrawCountRef {
    uint draw_count;
};

layout(push_constant) uniform PushConstants {
    uint max_draws;
    float cutoff_ratio;
    uint64_t count_addr;
} pc;

void main() {
    // cutoff_ratio 可能为负/NaN（作者误配 TOML），float→uint 转换未定义 ⇒ 先钳制到 [0,1]
    float ratio = pc.cutoff_ratio;
    if (!(ratio > 0.0))
        ratio = 0.0;      // 同时兜住负数与 NaN
    ratio = min(ratio, 1.0);

    uint count = uint(round(float(pc.max_draws) * ratio));
    if (count > pc.max_draws)
        count = pc.max_draws;

    DrawCountRef counter = DrawCountRef(pc.count_addr);
    counter.draw_count = count;
}
