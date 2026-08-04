// @ulre begin
// @ulre name vertex_fetch_ssbo
// @ulre kind VertexInput
// @ulre priority 0
// @ulre end
// SSBO 顶点获取模块
#include "common/descriptor_macros.glsl"

struct VertexData
{
    vec3 position;
    vec3 normal;
    vec2 uv0;
    // 按需扩展 (tangent, color, etc.)
};

layout(set=VERTEX_DATA_SET, binding=VTX_DATA_BINDING) readonly buffer VertexDataBuffer { VertexData vertices[]; };
layout(set=VERTEX_DATA_SET, binding=IDX_DATA_BINDING) readonly buffer IndexDataBuffer { uint indices[]; };

vec3 FetchPosition(uint vertexIndex) { return vertices[vertexIndex].position; }
vec3 FetchNormal(uint vertexIndex) { return vertices[vertexIndex].normal; }
vec2 FetchUV0(uint vertexIndex) { return vertices[vertexIndex].uv0; }
