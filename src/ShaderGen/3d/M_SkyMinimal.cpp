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

    // Fragment shader: minimal atmosphere + sun/moon disc + halo
    // Uses interpolated Input.Direction as view direction in local space
    constexpr const char fs_main[] = R"(
vec3 tonemap_exp2(vec3 x) { return exp2(-x); }

void main()
{
    // Use interpolated local direction (sphere vertex position normalized)
    vec3 dir = normalize(Input.Direction);

    vec3 sky_color=exp2(-dir.z/sky.base_sky_color.rgb);

    // Sun / Moon rendering
    vec3 to_light = normalize(-sky.sun_direction.xyz);
    float cos_t = clamp(dot(dir, to_light), -1.0, 1.0);

    const float sun_rad = radians(sky.sun_ang_deg);
    const float sun_cos = cos(sun_rad);

    float core = smoothstep(sun_cos, 1.0, cos_t);
    float halo = exp2((cos_t - 1.0) * 16.0) * sky.halo_intensity;

    vec3 sun_core = sky.sun_color.rgb * core * sky.sun_intensity;
    vec3 moon_core = sky.moon_color.rgb * core * sky.moon_intensity;

    vec3 color = sky_color + (sun_core + moon_core) + halo * sky.halo_color.rgb;

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
