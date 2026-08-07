// CubeMap environment surface. sky_cubemap.glsl is injected by the
// SkyLightCubeMap resource manifest before this surface function.
#include "common/surface_interface.glsl"

SurfaceOutput EvalSurface(SurfaceInput si, uint dataIndex)
{
    SurfaceOutput so;
    so.baseColor = SampleSkyCubemap(si.worldPos);
    so.normal = vec3(0.0, 0.0, 1.0);
    so.metallic = 0.0;
    so.roughness = 1.0;
    so.fresnel = 0.0;
    so.ao = 1.0;
    so.emissive = vec3(0.0);
    so.alpha = 1.0;
    return so;
}
