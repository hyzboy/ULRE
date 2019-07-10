#version 450 core

layout(location = 0) in vec3 Vertex;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec3 Tangent;

layout(binding = 0) uniform WorldMatrix
{
    mat4 two_dim;
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    vec4 view_pos;
} world;

layout(push_constant) uniform Consts {
    mat4 local_to_world;
} pc;

layout(location = 0) out vec3 FragmentNormal;
layout(location = 1) out vec3 FragmentTangent;
layout(location = 2) out vec3 FragmentPosition;
layout(location = 3) out vec2 FragmentTexCoord;

void main()
{
    vec4 pos=vec4(Vertex,1.0)*pc.local_to_world;

    gl_Position=pos*world.mvp;

    FragmentPosition=pos.xyz;
    FragmentTexCoord=TexCoord;

//    mat3 n=inverse(mat3(pc.local_to_world));

//    Normal.y=-Normal.y;
//    FragmentNormal=normalize(Normal)*n;
//    FragmentTangent=normalize(Tangent)*n;

    FragmentNormal=normalize((Normal+vec3(1.0))/2.0);
    FragmentTangent=Tangent;
}
