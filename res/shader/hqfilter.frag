#version 450 core

layout(binding = 2) uniform sampler2D tex;

layout(location = 0) in vec2 FragmentTexCoord;
layout(location = 0) out vec4 FragColor;

vec4 hqfilter(sampler2D samp, vec2 tc)
{
    // Get the size of the texture we'll be sampling from
    vec2 texSize = textureSize(samp, 0);

    // Scale our input texture coordinates up, move to center of texel
    vec2 uvScaled = tc * texSize + 0.5;

    // Find integer and fractional parts of texture coordinate
    vec2 uvInt = floor(uvScaled);
    vec2 uvFrac = fract(uvScaled);

    // Replace fractional part of texture coordinate
    uvFrac = smoothstep(0.0, 1.0, uvFrac);

    // Reassemble texture coordinate, remove bias, and
    // scale back to 0.0 - 1.0 ranage
    vec2 uv = (uvInt + uvFrac - 0.5) / texSize;

    // Regular texture lookup
    return texture(samp, uv);
}

void main()
{
    FragColor=hqfilter(tex,FragmentTexCoord);
}
