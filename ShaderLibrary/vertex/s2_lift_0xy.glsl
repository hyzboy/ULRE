// @ulre begin
// @ulre name s2_lift_0xy
// @ulre kind Position
// @ulre priority 0
// @ulre require GeometryAttribute Position Float 2 2
// @ulre end
// Stage 2: Lift XY → (0, X, Y, 1)
// 2D position mapped to the YZ plane.
// Requires: stage-1 declares `layout(location=0) in vec2 Position`.
vec4 GetLocalPos() { return vec4(0.0, Position, 1.0); }
