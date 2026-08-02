// Stage 3 helper: orient_world — provides GetL2W() for standard world transform.
// Requires (already declared by caller): L2W_SSBO + L2W_INDEX_ROWS_SSBO

#ifndef HELPER_ORIENT_WORLD_GLSL
#define HELPER_ORIENT_WORLD_GLSL

mat4 GetL2W()
{
    uint transformID = ResolveTransformID(gl_InstanceIndex);
    return l2w.mats[transformID];
}

#endif // HELPER_ORIENT_WORLD_GLSL
