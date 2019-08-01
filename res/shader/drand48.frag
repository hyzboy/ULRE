#version 450 core

layout(location = 0) out vec4 FragColor;

/* returns a varying number between 0 and 1 */
float drand48(vec2 co) 
{
  return 2 * fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453) - 1;
}

void main()
{
    float gray=drand48(gl_FragCoord.xy);

    FragColor=vec4(vec3(gray),1.0);
}
