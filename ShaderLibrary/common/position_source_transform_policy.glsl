// R08 stage split helpers:
// - PositionSource: produce local/object-space positions
// - TransformPolicy: apply facing / fixed-screen-scale / clip transform

vec3 PositionSourceQuadLocal(vec3 position_attr)
{
    return vec3(position_attr.xy, 0.0);
}

vec3 PositionSourceObjectOrigin(mat4 l2w_mat)
{
    return (l2w_mat * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
}

vec3 TransformPolicyCameraFacing(vec3 world_center,
                                 vec3 local_pos,
                                 vec3 camera_right,
                                 vec3 camera_up)
{
    return world_center
         + local_pos.x * camera_right
         + local_pos.y * camera_up;
}

vec4 TransformPolicyApplyVP(mat4 vp, vec3 world_pos)
{
    return vp * vec4(world_pos, 1.0);
}

vec4 TransformPolicyFixedScreenScale(vec4 center_clip,
                                     vec2 local_xy,
                                     vec2 normalized_screen_scale)
{
    vec2 center_ndc = center_clip.xy / center_clip.w;
    vec2 ndc = center_ndc + local_xy * normalized_screen_scale;
    return vec4(ndc * center_clip.w, center_clip.z, center_clip.w);
}
