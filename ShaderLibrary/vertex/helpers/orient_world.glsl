// @ulre begin
// @ulre name orient_world
// @ulre kind Utility
// @ulre priority 0
// @ulre end
// Stage 3 helper: orient_world — provides GetL2W() for standard world transform.
// Requires (already injected by CompileCompositorMaterial): L2W_SSBO + l2w_index_rows + ResolveTransformID

#ifndef HELPER_ORIENT_WORLD_GLSL
#define HELPER_ORIENT_WORLD_GLSL

#if defined(HGL_L2W_FROM_VERTEX_ATTR)
// PattleColor-style materials: L2W resolved from the TransformID vertex attribute.
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
