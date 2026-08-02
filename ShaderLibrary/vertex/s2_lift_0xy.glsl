// Stage 2: Lift XY → (0, X, Y, 1)
// 2D position mapped to the YZ plane.
// Requires: s1_input_vec2
vec4 GetLocalPos() { return vec4(0.0, Position, 1.0); }
