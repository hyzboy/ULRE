// @ulre begin
// @ulre name s3_clip_passthrough
// @ulre kind Transform
// @ulre priority 0
// @ulre end
// Stage 3: Clip Passthrough — local_pos is already in clip space, pass through.
// Used for NDC-input and ZeroToOne materials whose Stage 2 already produces clip coords.

vec4 GetClipPos(vec4 local_pos)
{
    return local_pos;
}
