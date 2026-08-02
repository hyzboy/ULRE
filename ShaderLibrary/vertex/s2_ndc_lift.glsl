// Stage 2: NDC Lift — input is already NDC XY coords, lift to clip-space XYZ.
// Requires: stage-1 declares `layout(location=0) in vec2 Position`.
vec4 GetLocalPos() { return vec4(Position, 0.0, 1.0); }
