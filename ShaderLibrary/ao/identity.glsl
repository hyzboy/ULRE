// @ulre begin
// @ulre name identity_ao
// @ulre kind Utility
// @ulre priority 0
// @ulre slot ambient_occlusion_provider
// @ulre provides_capability ambient_occlusion
// @ulre end

#ifndef IDENTITY_AO_GLSL
#define IDENTITY_AO_GLSL

float GetAmbientOcclusion(SurfaceInput surface)
{
    return 1.0;
}

#endif
