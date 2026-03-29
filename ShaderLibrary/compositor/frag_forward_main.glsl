// --------------------------------------------------------------------------
// frag_forward_main.glsl - Unified forward fragment main template.
//
// Control defines (set before #including this file):
//
//   Varying flags (must match the vertex shader's HAS_* defines):
//     HAS_WORLD_POS      fragWorldPos (vec3)      - also drives si.viewDir
//     HAS_WORLD_NORMAL   fragWorldNormal (vec3)
//     HAS_UV0            fragUV0 (vec2)
//     HAS_VERTEX_COLOR   fragVertexColor (vec4)
//     HAS_TEXCOORD       fragTexCoord (vec2)       - maps to si.uv0
//     HAS_DIRECTION      fragDirection (vec3)      - maps to si.worldPos+viewDir; pulls ubo_sky
//     HAS_LUMINANCE      fragLuminance (float)
//     HAS_CLIP_POS       fragClipPos (vec4)        - maps to si.worldPos; pulls ubo_camera
//
//   UBO flags:
//     NEEDS_SKY          include ubo_sky.glsl before SURFACE_FUNCTION_FILE
//     NEEDS_CAMERA       include ubo_camera.glsl before SURFACE_FUNCTION_FILE
//
//   Feature flags:
//     ENABLE_LIGHTING    EvalLighting path (pulls camera+sky+viewport UBOs);
//                        si.viewDir = normalize(camera.pos - fragWorldPos)
//     ALPHA_MODE_MASKED  alpha-test discard at ALPHA_THRESHOLD (default 0.5)
//     ALPHA_MODE_DITHER  Bayer 4x4 ordered dither discard
//     (default)          transparent passthrough: output so.baseColor + so.alpha
//
//   Injected macro (replaced by C++ assembler before compilation):
//     SURFACE_FUNCTION_FILE  path to the surface .glsl file
// --------------------------------------------------------------------------

// Fragment output
layout(location=0) out vec4 outColor;

// 4. Lighting helper - injected via LIGHTING_FUNCTION_FILE by C++ assembler
//    (replaces old #include "common/lighting.glsl")

// 7. Bayer 4x4 ordered dither helper
#ifdef ALPHA_MODE_DITHER
float BayerDither4x4(ivec2 p)
{
    const float bayer[16] = float[16](
         0.0 / 16.0,  8.0 / 16.0,  2.0 / 16.0, 10.0 / 16.0,
        12.0 / 16.0,  4.0 / 16.0, 14.0 / 16.0,  6.0 / 16.0,
         3.0 / 16.0, 11.0 / 16.0,  1.0 / 16.0,  9.0 / 16.0,
        15.0 / 16.0,  7.0 / 16.0, 13.0 / 16.0,  5.0 / 16.0
    );
    int idx = (p.y % 4) * 4 + (p.x % 4);
    return bayer[idx];
}
#endif

// 8. Alpha threshold default for ALPHA_MODE_MASKED
#ifdef ALPHA_MODE_MASKED
#  ifndef ALPHA_THRESHOLD
#    define ALPHA_THRESHOLD 0.5
#  endif
#endif

void main()
{
#ifdef TEXTURE_ARRAY_MODE
    _ULRE_InitTextureLayerIndices(MATERIAL_INSTANCE_ID_OVERRIDE);
#endif

    SurfaceInput si;

    // worldPos
#if defined(HAS_WORLD_POS)
    si.worldPos    = fragWorldPos;
#elif defined(HAS_CLIP_POS)
    si.worldPos    = fragClipPos.xyz;
#elif defined(HAS_DIRECTION)
    si.worldPos    = fragDirection;
#else
    si.worldPos    = vec3(0.0);
#endif

    // worldNormal
#ifdef HAS_WORLD_NORMAL
    si.worldNormal = normalize(fragWorldNormal);
#else
    si.worldNormal = vec3(0.0, 0.0, 1.0);
#endif

    // uv0
#if defined(HAS_UV0)
    si.uv0         = fragUV0;
#elif defined(HAS_TEXCOORD)
    si.uv0         = fragTexCoord;
#else
    si.uv0         = vec2(0.0);
#endif

    si.uv1         = vec2(0.0);

    // vertexColor + luminance base
#ifdef HAS_VERTEX_COLOR
    si.vertexColor = fragVertexColor;
    si.luminance   = 1.0;
#else
    si.vertexColor = vec4(1.0);
    si.luminance   = 0.0;
#endif

    // luminance override from varying
#ifdef HAS_LUMINANCE
    si.luminance   = fragLuminance;
#endif

    // viewDir
#ifdef ENABLE_LIGHTING
    si.viewDir     = normalize(camera.pos - fragWorldPos);
#elif defined(HAS_WORLD_POS)
    si.viewDir     = normalize(-fragWorldPos);
#elif defined(HAS_DIRECTION)
    si.viewDir     = fragDirection;
#else
    si.viewDir     = vec3(0.0, 0.0, 1.0);
#endif

    si.screenPos   = gl_FragCoord.xy;

    SurfaceOutput so = EvalSurface(si);

    // Output based on alpha mode
#ifdef ENABLE_LIGHTING
    vec3 litColor = EvalLighting(so, si.viewDir, ULRE_GetSkyLightDir(), ULRE_GetSkyLightColor());
    litColor += so.baseColor * ULRE_GetSkyAmbientColor() * so.ao;
    litColor += so.emissive;
    outColor = vec4(litColor, so.alpha);
#elif defined(ALPHA_MODE_MASKED)
    if (so.alpha < ALPHA_THRESHOLD) discard;
    outColor = vec4(so.baseColor, 1.0);
#elif defined(ALPHA_MODE_DITHER)
    float threshold = BayerDither4x4(ivec2(gl_FragCoord.xy));
    if (so.alpha < threshold) discard;
    outColor = vec4(so.baseColor, 1.0);
#else
    outColor = vec4(so.baseColor, so.alpha);
#endif
}
