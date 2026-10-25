// vertex_color_bindings.glsl — VertexColor / VertexPalette 两个域的共享数据布局。
//
// 这两个领域**不读材质私有数据行**：颜色完全来自顶点色 varying
// （`material/vertex_color_source.glsl`）。所以既没有 `MTL_ROW`，也没有贴图表，
// 更没有采样器宏——契约里这两个领域的贴图槽表是空的。

#ifndef HGL_DOMAIN_VERTEX_COLOR_BINDINGS_GLSL
#define HGL_DOMAIN_VERTEX_COLOR_BINDINGS_GLSL

#endif // HGL_DOMAIN_VERTEX_COLOR_BINDINGS_GLSL
