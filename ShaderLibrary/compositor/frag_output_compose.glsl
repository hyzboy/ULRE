// compositor/frag_output_compose.glsl — Write outColor from SurfaceOutput + alpha mode
//
// Context requirements:
//   - layout(location=0) out vec4 outColor;  (declared by caller)
//   - common/surface_interface.glsl  (SurfaceOutput)
//   - If ENABLE_LIGHTING: EvalLighting(), ULRE_GetSkyLightDir/Color/AmbientColor()
//   - If ALPHA_MODE_MASKED or ALPHA_MODE_DITHER: util/alpha_test.glsl
//
// Usage:
//   SurfaceInput  si = ResolveSurfaceInput();
//   SurfaceOutput so = EvalSurface(si);
//   ComposeOutput(so, si);

#ifndef ULRE_COMPOSITOR_FRAG_OUTPUT_COMPOSE_GLSL
#define ULRE_COMPOSITOR_FRAG_OUTPUT_COMPOSE_GLSL

void ComposeOutput(SurfaceOutput so, SurfaceInput si)
{
#ifdef ENABLE_LIGHTING
    vec3 litColor  = EvalLighting(so, si.viewDir, ULRE_GetSkyLightDir(), ULRE_GetSkyLightColor());
    litColor      += so.baseColor * ULRE_GetSkyAmbientColor() * so.ao;
    litColor      += so.emissive;
    outColor = vec4(litColor, so.alpha);
#elif defined(ALPHA_MODE_MASKED)
    AlphaTestDiscard(so.alpha);
    outColor = vec4(so.baseColor, 1.0);
#elif defined(ALPHA_MODE_DITHER)
    DitherDiscard(so.alpha, gl_FragCoord.xy);
    outColor = vec4(so.baseColor, 1.0);
#else
    outColor = vec4(so.baseColor, so.alpha);
#endif
}

#endif // ULRE_COMPOSITOR_FRAG_OUTPUT_COMPOSE_GLSL
