// Common utility functions
// Mathematical constants and helper functions

const float PI = 3.14159265359;
const float INV_PI = 0.31830988618;

// Clamp a value between 0 and 1
float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

vec3 saturate(vec3 x)
{
    return clamp(x, vec3(0.0), vec3(1.0));
}

// Convert linear to sRGB
vec3 linearToSRGB(vec3 color)
{
    return pow(color, vec3(1.0 / 2.2));
}

// Convert sRGB to linear
vec3 sRGBToLinear(vec3 color)
{
    return pow(color, vec3(2.2));
}
