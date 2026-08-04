// @ulre begin
// @ulre name s2_pixel_to_local
// @ulre kind Position
// @ulre priority 0
// @ulre require GeometryAttribute Position Float 2 2
// @ulre end
// Stage 2: Pixel to Local — input is pixel-space coordinates (Ortho 2D).
// No transform applied here; projection stage will apply the ortho matrix.
// Requires: stage-1 declares `layout(location=0) in vec2 Position`.
vec4 GetLocalPos() { return vec4(Position, 0.0, 1.0); }
