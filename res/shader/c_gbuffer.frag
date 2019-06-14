#version 450 core

layout (binding = 1) uniform sampler2D samplerAlbedo;
layout (binding = 2) uniform sampler2D samplerNormalMap;

layout(location = 0) in vec3 FragmentColor;
layout(location = 1) in vec3 FragmentNormal;
layout(location = 2) in vec3 FragmentTangent;
layout(location = 3) in vec3 FragmentWorldPos;
layout(location = 4) in vec2 FragmentTexCoord;

layout (location = 0) out vec4 FragAlbedo;
layout (location = 1) out vec4 FragNormal;
layout (location = 2) out vec4 FragWorldPos;

void main()
{
    FragWorldPos=vec4(FragmentWorldPos,1.0);    
    FragAlbedo  =texture(samplerAlbedo,FragmentTexCoord)*vec4(FragmentColor,1.0);
    
    vec3 N = normalize(FragmentNormal);
    N.y = -N.y;
    vec3 T = normalize(FragmentTangent);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 tnorm = TBN * normalize(texture(samplerNormalMap, FragmentTexCoord).xyz * 2.0 - vec3(1.0));
    FragNormal = vec4(tnorm, 1.0);
}