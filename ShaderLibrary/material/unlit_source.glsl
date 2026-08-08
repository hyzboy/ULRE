// @ulre begin
// @ulre name unlit_source
// @ulre kind Utility
// @ulre priority 0
// @ulre require Resource MaterialData
// @ulre ssbo mtl EmissiveSurface 0 Fragment optional fallback
// @ulre end
// Unlit material source provider — reads emissive color from SSBO.
// Used by unlit_color3d_surface and unlit_luminance_surface.

#ifndef UNLIT_SOURCE_GLSL
#define UNLIT_SOURCE_GLSL

EmissiveSurfaceData EvalUnlitSource(uint dataIndex)
{
    return MTL_DATA.data[dataIndex];
}

#endif // UNLIT_SOURCE_GLSL
