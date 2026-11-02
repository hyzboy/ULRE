#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64) in;

// 缓冲区地址经 push constant 下发（BDA），无 set 无 binding
layout(buffer_reference, std430, buffer_reference_align=16) buffer FloatArrayRef { float data[]; };

layout(push_constant) uniform PushConstants {
    uint64_t in_addr;
    uint64_t out_addr;
    uint element_count;
} pc;

void main()
{
    const uint idx = gl_GlobalInvocationID.x;

    // 派发组数通常向上取整，尾部组越界读写会破坏相邻内存 ⇒ 按元素数早退
    if (idx >= pc.element_count)
        return;

    FloatArrayRef in_buf  = FloatArrayRef(pc.in_addr);
    FloatArrayRef out_buf = FloatArrayRef(pc.out_addr);

    out_buf.data[idx] = in_buf.data[idx] * in_buf.data[idx];
}
