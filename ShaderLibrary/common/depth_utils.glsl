
/* Comment normalized for ASCII safety. */
float LinearizeDepth(float d, float near_z)
{
    return near_z / d;  }

/* Comment normalized for ASCII safety. */
vec3 ReconstructWorldPos(vec2 ndc, float depth, mat4 inv_view_proj)
{
    vec4 clip = vec4(ndc * 2.0 - 1.0, depth, 1.0);
    vec4 world = inv_view_proj * clip;
    return world.xyz / world.w;
}
