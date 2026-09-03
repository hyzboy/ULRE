// @ulre begin
// @ulre name identity_shadow
// @ulre kind Utility
// @ulre priority 0
// @ulre slot shadow_provider
// @ulre provides_capability shadow_factor
// @ulre end

#ifndef IDENTITY_SHADOW_GLSL
#define IDENTITY_SHADOW_GLSL

float GetShadowFactor(SurfaceInput surface)
{
    return 1.0;
}

#endif
