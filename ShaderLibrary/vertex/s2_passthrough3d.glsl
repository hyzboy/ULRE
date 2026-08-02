// Stage 2: Passthrough 3D — standard 3D position, passes through as-is.
// Requires: s1_input_vec3 (or any stage-1 with vec3 Position)
vec4 GetLocalPos() { return vec4(Position, 1.0); }
