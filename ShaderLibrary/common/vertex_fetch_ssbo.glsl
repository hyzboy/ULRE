// SSBO 顶点获取模块
struct VertexData
{
    vec3 position;
    vec3 normal;
    vec2 uv0;
    // 按需扩展 (tangent, color, etc.)
};

layout(set=3, binding=18) readonly buffer VertexDataBuffer { VertexData vertices[]; };
layout(set=3, binding=19) readonly buffer IndexDataBuffer { uint indices[]; };

vec3 FetchPosition(uint vertexIndex) { return vertices[vertexIndex].position; }
vec3 FetchNormal(uint vertexIndex) { return vertices[vertexIndex].normal; }
vec2 FetchUV0(uint vertexIndex) { return vertices[vertexIndex].uv0; }
