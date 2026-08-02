// Stage 2: ZeroToOne → NDC — input [0,1] range, maps to NDC [-1,1] before lifting.
// Requires: s1_input_vec2
vec4 GetLocalPos() { return vec4(Position * 2.0 - 1.0, 0.0, 1.0); }
