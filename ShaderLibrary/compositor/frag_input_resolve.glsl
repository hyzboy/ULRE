// compositor/frag_input_resolve.glsl — Populate SurfaceInput from fragment varyings
//
// Context requirements (must be in scope before including this file):
//   - varying declarations from common/varying_interface.glsl (HAS_* variants)
//   - common/ubo_camera.glsl  (if ENABLE_LIGHTING is defined)
//   - common/surface_interface.glsl  (SurfaceInput struct)
//
// The returned SurfaceInput is ready to pass to EvalSurface().

#ifndef ULRE_COMPOSITOR_FRAG_INPUT_RESOLVE_GLSL
#define ULRE_COMPOSITOR_FRAG_INPUT_RESOLVE_GLSL

SurfaceInput ResolveSurfaceInput()
{
    SurfaceInput si;

    // worldPos
#if defined(HAS_POSITION)
    si.worldPos    = fragWorldPos;
#elif defined(HAS_CLIP_POS)
    si.worldPos    = fragClipPos.xyz;
#elif defined(HAS_DIRECTION)
    si.worldPos    = fragDirection;
#else
    si.worldPos    = vec3(0.0);
#endif

    // worldNormal
#ifdef HAS_NORMAL
    si.worldNormal = normalize(fragWorldNormal);
#else
    si.worldNormal = vec3(0.0, 0.0, 1.0);
#endif

    // worldTangent (xyz + handedness sign in w)
#ifdef HAS_TANGENT
    si.worldTangent = vec4(normalize(fragWorldTangent.xyz), fragWorldTangent.w);
#else
    si.worldTangent = vec4(1.0, 0.0, 0.0, 1.0);
#endif

    // uv0
#if defined(HAS_TEXCOORD)
    si.uv0         = fragUV0;
#else
    si.uv0         = vec2(0.0);
#endif

    si.uv1 = vec2(0.0);

    // vertexColor + luminance base
#ifdef HAS_COLOR
    si.vertexColor = fragVertexColor;
    si.luminance   = 1.0;
#else
    si.vertexColor = vec4(1.0);
    si.luminance   = 0.0;
#endif

#ifdef HAS_LUMINANCE
    si.luminance = fragLuminance;
#endif

    // viewDir
#ifdef ENABLE_LIGHTING
    si.viewDir = normalize(camera.pos - fragWorldPos);
#elif defined(HAS_POSITION)
    si.viewDir = normalize(-fragWorldPos);
#elif defined(HAS_DIRECTION)
    si.viewDir = fragDirection;
#else
    si.viewDir = vec3(0.0, 0.0, 1.0);
#endif

    si.screenPos = gl_FragCoord.xy;
    return si;
}

#endif // ULRE_COMPOSITOR_FRAG_INPUT_RESOLVE_GLSL
