// checker_2d_bindings.glsl — Checker2D 域**有意为空**的数据布局。
//
// 该领域契约里贴图槽表为空、`material_private_data` 也不被消费：棋盘格图案
// 由 `module/provider/material_source/material_source_checkerboard_2d.glsl` 自行算出。因此这里没有
// `MTL_ROW`、没有 `MaterialTextureReferencesRef`、也没有采样器宏。
//
// 文件刻意保留而不是让主着色器少一行 `#include`：它把「本领域无数据行/
// 无贴图」写成**可见的物理事实**，与 VertexColor 的处理同构；将来若契约新增
// 槽位，改动点只有这一个文件。

#ifndef HGL_DOMAIN_CHECKER_2D_BINDINGS_GLSL
#define HGL_DOMAIN_CHECKER_2D_BINDINGS_GLSL

#endif // HGL_DOMAIN_CHECKER_2D_BINDINGS_GLSL
