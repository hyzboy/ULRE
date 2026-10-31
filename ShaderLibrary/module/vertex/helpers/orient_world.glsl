// @ulre begin
// @ulre name orient_world
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// Stage 3 helper: orient_world — provides GetL2W() for standard world transform.
// Requires (already injected by CompileCompositorMaterial): L2W_SSBO + l2w_index_rows + ResolveTransformID

#ifndef HELPER_ORIENT_WORLD_GLSL
#define HELPER_ORIENT_WORLD_GLSL

// 两种 L2W 来源靠**装了哪个模块**区分，不靠编译期分支：
//   · 装了 `module/vertex/stream_transform_id.glsl`（调色板材质）⇒ 该模块自定义
//     `HGL_L2W_FROM_VERTEX_ATTR` ⇒ 逐顶点查 `l2w.mats[TransformID]`；
//   · 没装 ⇒ 按实例号过 l2w_index 表（注入方：CompileCompositorMaterial）。
//
// 这个 `#ifdef` 是**模块在位性**的分支，不是配置选择：没有垫片能同时给出
// 两条 L2W 语义，声明哪一条由属性决定。此前生成器在 C++ 侧挑分支发射，
// 现在选择权下移到模块自身，生成器残留因此彻底消失。
#ifdef HGL_L2W_FROM_VERTEX_ATTR
// Palette-color materials: L2W resolved from the TransformID vertex attribute.
// Requires: `layout(location=N) in uint TransformID;`
mat4 GetL2W()
{
    return l2w.mats[TransformID];
}
#else
// Standard path: L2W resolved from gl_InstanceIndex through the l2w_index_rows
// table (injected by CompileCompositorMaterial).
mat4 GetL2W()
{
    uint transformID = ResolveTransformID(gl_InstanceIndex);
    return l2w.mats[transformID];
}
#endif // HGL_L2W_FROM_VERTEX_ATTR

#endif // HELPER_ORIENT_WORLD_GLSL
