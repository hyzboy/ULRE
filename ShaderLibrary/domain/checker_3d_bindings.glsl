// checker_3d_bindings.glsl — Checker3D 域**有意为空**的数据布局。
//
// 与 Checker2D 同族：图案由 `material/checkerboard_3d_source.glsl` 按
// **世界坐标**算出，不读材质私有数据行、不读贴图 ⇒ 没有 `MTL_ROW`、
// 没有贴图表、没有采样器宏。
//
// 唯一差别在顶点侧：本领域要世界坐标，所以主着色器多一个
// `fragWorldPos`（location 1）输入，`materialDataIndex` 仍是字面 `0u`。

#ifndef HGL_DOMAIN_CHECKER_3D_BINDINGS_GLSL
#define HGL_DOMAIN_CHECKER_3D_BINDINGS_GLSL

#endif // HGL_DOMAIN_CHECKER_3D_BINDINGS_GLSL
