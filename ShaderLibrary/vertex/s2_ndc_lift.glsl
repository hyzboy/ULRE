// @ulre begin
// @ulre name s2_ndc_lift
// @ulre kind Position
// @ulre priority 0
// @ulre require GeometryAttribute Position Float 2 2
// @ulre end
// Stage 2: NDC Lift — input is already NDC XY coords, lift to clip-space XYZ.
// Requires: stage-1 declares `layout(location=0) in vec2 Position`.
vec4 GetLocalPos() { return vec4(Position, 0.0, 1.0); }
