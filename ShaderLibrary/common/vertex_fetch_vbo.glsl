#ifndef ULRE_COMMON_VERTEX_FETCH_VBO_GLSL
#define ULRE_COMMON_VERTEX_FETCH_VBO_GLSL

vec3 FetchPosition(uint vertexIndex) { return inPosition; }
vec3 FetchNormal(uint vertexIndex)   { return inNormal; }
vec2 FetchUV0(uint vertexIndex)      { return inUV0; }

#endif // ULRE_COMMON_VERTEX_FETCH_VBO_GLSL
