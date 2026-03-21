
layout(set=PERMATERIAL_SET, binding=TEX_BASECOLOR_BINDING) uniform sampler2DArray TextureBaseColor;

layout(location=0) flat in uint fragLayer;
layout(location=1) in vec2 fragTexCoord;

layout(location=0) out vec4 FragColor;

void main()
{
    FragColor = texture(TextureBaseColor, vec3(fragTexCoord, fragLayer));
}
