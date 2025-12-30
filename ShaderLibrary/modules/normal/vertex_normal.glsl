// Vertex Normal
// Use vertex normal directly
// Interface: GetNormal()
// Requires: Input.Normal
// Dependencies: none

vec3 GetNormal()
{
    return normalize(Normal);
}
