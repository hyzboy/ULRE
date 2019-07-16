#version 450

layout(binding = 0) uniform WorldMatrix
{
	vec2 vp_size;
    mat4 ortho;
    mat4 projection;
    mat4 modelview;
    mat4 mvp;
    vec4 view_pos;
} world;

layout (location = 0) out vec4 outFragcolor;

void main() 
{  
	outFragcolor = vec4(gl_FragCoord.rg/world.vp_size,0.0,1.0);
}