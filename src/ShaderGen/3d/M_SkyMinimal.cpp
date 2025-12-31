#include "Std3DMaterial.h"
#include <hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // Vertex shader: pass through position and output local direction
    constexpr const char vs_main[] = R"(
void main()
{
    // Position is the vertex attribute in local space
    Output.Direction = normalize(Position);

    gl_Position = GetPosition3D();
})";

    constexpr const char fs_main[] = R"(
vec3 getSky(vec3 dir, vec3 to_light)
{
    // Zenith factor: 1 at zenith, 0 at horizon
    float h = clamp(dir.z, 0.0, 1.0);

    // Base gradient (Rayleigh-like), brighter toward zenith
    vec3 base = sky.base_sky_color.rgb;
    vec3 grad = base * exp2(-(1.0 - h) * 0.8);

    // Sunward warm scattering region (broader than disc)
    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    // Expand region for glow around sun (e.g., ~6x disc radius)
    float region = smoothstep(cos(sun_rad * 6.0), 1.0, cos_t);

    // Warmer near horizon; blend with sun proximity and sun intensity
    float horizon = 1.0 - h;
    vec3 warmTint = mix(vec3(1.0), vec3(1.0, 0.4, 0.05) * 1.2, clamp(horizon, 0.0, 1.0));
    vec3 scatterMix = mix(grad, warmTint, region * 0.5 * sky.sun_intensity);

    // Final sky color blends more haze near horizon
    float atmosphere = sqrt(max(0.0, 1.0 - h));
    return mix(grad, scatterMix, atmosphere * 0.7);
}

// Stylized sun (disc + glow)
vec3 getSun(vec3 dir, vec3 to_light)
{
    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);
    float sun_rad = radians(sky.sun_ang_deg);
    float sun_cos = cos(sun_rad);

    float coreN = clamp((cos_t - sun_cos) / max(1e-5, 1.0 - sun_cos), 0.0, 1.0);
    float h = clamp(dir.z, 0.0, 1.0);

    float hard = pow(coreN, 100.0);
    hard *= pow(h, 1.0 / 1.65);

    float glow = pow(coreN, 6.0);
    glow *= pow(h, 0.5);

    float sunMask = clamp(hard + glow, 0.0, 1.0);
    return sky.sun_color.rgb * sunMask * sky.sun_intensity;
}

void main()
{
    // Use interpolated local direction (sphere vertex position normalized)
    vec3 dir = normalize(Input.Direction);

    // Sun / Moon rendering using direction space
    vec3 to_light = normalize(sky.sun_direction.xyz);

    // Sky color from adapted function
    vec3 sky_color = getSky(dir, to_light);

    // Stylized sun
    vec3 sun_color = getSun(dir, to_light);

    FragColor = vec4(sky_color+sun_color, 1.0);
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

            // export a Direction vec3 from vertex to fragment
            vsc->AddOutput(SVT_VEC3, "Direction");

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
