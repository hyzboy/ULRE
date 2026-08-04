// @ulre begin
// @ulre name s2_passthrough3d
// @ulre kind Position
// @ulre priority 0
// @ulre require GeometryAttribute Position Float 3 3
// @ulre end
// Stage 2: Passthrough 3D — standard 3D position, passes through as-is.
// Requires: stage-1 declares `layout(location=0) in vec3 Position`.
vec4 GetLocalPos() { return vec4(Position, 1.0); }
