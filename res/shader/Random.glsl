vec3 random(vec2 uv)        // from Godot 3.2
{
    return vec3(fract(sin(dot(uv,vec2(12.9898,78.233)))*43758.5453123));
}
