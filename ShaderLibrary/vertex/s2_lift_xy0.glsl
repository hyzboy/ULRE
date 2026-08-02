// Stage 2: Lift XY → (X, Y, 0, 1)
// 2D position in the XY plane, lifts to 3D with Z=0.
// Requires: stage-1 declares `layout(location=0) in vec2 Position`.
vec4 GetLocalPos() { return vec4(Position, 0.0, 1.0); }
