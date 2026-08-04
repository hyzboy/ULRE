// @ulre begin
// @ulre name vertex_fetch_vbo
// @ulre kind VertexInput
// @ulre priority 0
// @ulre require GeometryAttribute Position Float 3 3
// @ulre require GeometryAttribute Normal Float 3 3
// @ulre require GeometryAttribute UV0 Float 2 2
// @ulre end
// VBO 顶点获取模块 — 使用传统 vertex attribute
// 注意：layout(location=N) 由 Compositor VS 声明，此文件仅提供与 SSBO 路径一致的函数别名

// 这些 `in` 变量在 Compositor VS 中已声明（非 SSBO 分支），此处不重复声明。
// 本文件仅为 vertex_fetch_ssbo.glsl 的 VBO 对等实现，
// 使 Surface Function 可以通过 FetchXxx() 统一接口获取顶点数据。

vec3 FetchPosition(uint vertexIndex) { return inPosition; }
vec3 FetchNormal(uint vertexIndex)   { return inNormal; }
vec2 FetchUV0(uint vertexIndex)      { return inUV0; }
