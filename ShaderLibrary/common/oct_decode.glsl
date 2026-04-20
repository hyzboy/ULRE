// common/oct_decode.glsl
// Octahedral normal decoding.
// Reference: Cigolle et al. "Survey of Efficient Representations for Independent Unit Vectors"
//
// Included automatically by the generated VS preamble when any vertex attribute
// uses an octahedral-encoded format (e.g. R16G16_SFLOAT or R8G8_UNORM normals).

/// Decode a signed-normalised vec2 (range [-1,1]) to a unit vec3 via the
/// octahedral mapping.
vec3 OctDecode(vec2 f)
{
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x -= (n.x >= 0.0) ? t : -t;
    n.y -= (n.y >= 0.0) ? t : -t;
    return normalize(n);
}
