// Stage 2: Passthrough 3D — standard 3D position, passes through as-is.
// Requires: stage-1 declares `layout(location=0) in vec3 Position`.
vec4 GetLocalPos() { return vec4(Position, 1.0); }
