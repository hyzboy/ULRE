// Stage 3: LocalToWorld only — legacy 2D NDC/ZeroToOne + HAS_L2W path.
// Applies only the LocalToWorld matrix, no camera VP.
// Requires: l2w SSBO, helpers/orient_world.glsl

#include "helpers/orient_world.glsl"

vec4 GetClipPos(vec4 local_pos)
{
    return GetL2W() * local_pos;
}
