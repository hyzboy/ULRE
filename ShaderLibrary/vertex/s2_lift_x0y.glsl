// Stage 2: Lift XY → (X, 0, Y, 1)
// 2D position in the ground plane (Z-up world), Y becomes world-Z height.
// Requires: s1_input_vec2
vec4 GetLocalPos() { return vec4(Position.x, 0.0, Position.y, 1.0); }
