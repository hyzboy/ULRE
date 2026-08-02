// Stage 2: NDC Lift — input is already NDC XY coords, lift to clip-space XYZ.
// Semantically same as LiftXY_XY0; used for CoordinateSystem2D::NDC materials.
// Requires: s1_input_vec2
vec4 GetLocalPos() { return vec4(Position, 0.0, 1.0); }
