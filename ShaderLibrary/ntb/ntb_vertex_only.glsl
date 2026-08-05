// @ulre begin
// @ulre name ntb_vertex_only
// @ulre kind Utility
// @ulre priority 0
// @ulre uses ntb_interface
// @ulre end
// NTB Vertex Only — 直接使用 VS 的顶点法线构造 NTB 空间，不使用法线贴图
#ifndef NTB_VERTEX_ONLY_GLSL
#define NTB_VERTEX_ONLY_GLSL

#include "common/ntb_interface.glsl"

NTBSpace EvalNTBSpace(SurfaceInput si, uint miID, float normalScale, uint normalTexHandle)
{
    return BuildOrthoNTB(si.worldNormal);
}

#endif // NTB_VERTEX_ONLY_GLSL
