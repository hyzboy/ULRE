#include "Std3DMaterial.h"
#include <hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // Vertex shader: just pass through position
    constexpr const char vs_main[] = R"(
void main()
{
    gl_Position = GetPosition3D();
})";

    // Fragment shader: minimal atmosphere + sun/moon disc + halo
    // Requires: viewport (inv_viewport_resolution), camera (inverse_vp, pos), sky (SkyInfo)
    constexpr const char fs_main[] = R"(
vec3 tonemap_exp2(vec3 x) { return exp2(-x); }

void main()
{
    // Reconstruct world ray direction from fragment coords
    vec2 uv = gl_FragCoord.xy * viewport.inv_viewport_resolution;      // 0..1
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);               // correct Y flip

    // Clip space position at far plane (z = 1)
    vec4 clip = vec4(ndc, 1.0, 1.0);

    // World position on far plane
    vec4 world_pos = camera.inverse_vp * clip;
    world_pos.xyz /= max(world_pos.w, 1e-6);

    // Ray from camera position to world_pos
    vec3 cam_pos = camera.pos;
    vec3 ray_dir = normalize(world_pos.xyz - cam_pos);

    // Base sky gradient: brighter near horizon using exp2(-ray_dir.z / base_sky_color)
    // Use Z-up convention: ray_dir.z in [0..1] means looking upward; horizon ~0
    vec3 base_col = sky.base_sky_color.rgb;
    vec3 vz = vec3(clamp(ray_dir.z, 0.0, 1.0));
    // component-wise argument divided by base color (avoid tiny div)
    vec3 grad_arg = vz / max(base_col, vec3(1e-3));
    vec3 sky_col = base_col * exp2(-grad_arg);

    // Sun / Moon rendering
    // sun_direction is from sun to scene, so vector to sun in world is -sun_direction.xyz
    vec3 to_light = normalize(-sky.sun_direction.xyz);
    float cos_t = clamp(dot(ray_dir, to_light), -1.0, 1.0);

    // Solar/lunar disc parameters
    const float sun_ang_deg = 0.53;                   // approximate sun angular diameter
    const float sun_rad = radians(sun_ang_deg);
    const float sun_cos = cos(sun_rad);

    // Core disc mask: 0 outside, 1 at center
    float core = smoothstep(sun_cos, 1.0, cos_t);

    // Halo: sharp near disc, exponential falloff
    float halo = exp2((cos_t - 1.0) * 16.0) * sky.halo_intensity;

    vec3 sun_core = sky.sun_color.rgb * core * sky.sun_intensity;

    // Night moon: use separate intensity/color (direction同样使用to_light)
    vec3 moon_core = sky.moon_color.rgb * core * sky.moon_intensity;

    // Combine
    vec3 color = sky_col + (sun_core + moon_core) + halo * sky.halo_color.rgb;

    FragColor = vec4(color, 1.0);
})";

    class MaterialSkyMinimal : public Std3DMaterial
    {
    public:
        using Std3DMaterial::Std3DMaterial;
        ~MaterialSkyMinimal() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if (!Std3DMaterial::CustomVertexShader(vsc))
                return false;

            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4, "FragColor");
            fsc->SetMain(fs_main);
            return true;
        }
    };
}

MaterialCreateInfo *CreateSkyMinimal(const VulkanDevAttr *dev_attr, const SkyMinimalCreateConfig *cfg)
{
    MaterialSkyMinimal m(cfg);
    return m.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
