#version 450

layout (binding = 1) uniform sampler2D TextureColor;
layout (binding = 2) uniform sampler2D TextureNormal;

layout(location = 0) in vec3 FragmentNormal;
layout(location = 1) in vec3 FragmentTangent;
layout(location = 2) in vec3 FragmentPosition;
layout(location = 3) in vec2 FragmentTexCoord;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outColor;

void main()
{
    outPosition=vec4(normalize(FragmentPosition*2.0-vec3(1.0)),1.0);

    vec3 N = normalize(FragmentNormal);
    vec3 T = normalize(FragmentTangent);
    vec3 B = cross(N,T);
    mat3 TBN = mat3(T,B,N);
    vec3 tnorm = (texture(TextureNormal,FragmentTexCoord).xyz*2.0-vec3(1.0))*TBN;

    outNormal=vec4(normalize(tnorm),1.0);
    //outNormal=vec4(normalize(FragmentNormal),1.0);

    outColor=texture(TextureColor,FragmentTexCoord);
}
