// Hejl Richard tone map
// see: http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 ToneMapping(vec3 color)
{
    color = max(vec3(0.0), color - vec3(0.004));
    return (color*(6.2*color+.5))/(color*(6.2*color+1.7)+0.06);
}
