// @ulre begin
// @ulre name stream_uv
// @ulre kind Utility
// @ulre priority 0
// @ulre provide UV0
// @ulre end
// Stage 1: UV 从独立 SSBO 读取——声明全局 TexCoord（主代码 fragUV0 = TexCoord）
// 提供 `void LoadUV()`，由 s1_position_* 的 LoadVertexData 顺序调用；
// 未装载本流时由 module/vertex/stream_defaults.glsl 的空实现补齐。
#ifndef S1_UV_GLSL
#define S1_UV_GLSL

// 流在位标记：告知 s1_stream_defaults 本流已提供 LoadUV()
#define S1_STREAM_UV 1

// BDA：UV 基址由 MeshDrawParams 行 addr_uv 携带（VertexUVRef 由
// MeshShaderVertexAdapter 集中声明），读法不变
#define sbo_vertex_uv VertexUVRef(draw_params.addr_uv)

vec2 TexCoord;

void LoadUV()
{
    TexCoord = sbo_vertex_uv.data[draw_params.vertex_base + VertexIndexID];
}

#endif // S1_UV_GLSL
