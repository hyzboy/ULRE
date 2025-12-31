#include "Std3DMaterial.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/UBOCommon.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // Vertex: pass through position, compute normal, forward TexCoord
    constexpr const char vs_main[] = R"(
void main()
{
    Output.TexCoord = TexCoord;
    Output.Normal   = GetNormal();
    Output.Position = GetPosition3D();
    gl_Position     = Output.Position;
})";

    // Fragment: textured Blinn-Phong + half-Lambert + Fresnel, lit by sky sun
    constexpr const char fs_main[] = R"(
vec3 halfLambert(vec3 n, vec3 l)
{
    float NdotL = max(dot(n, l), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    // Hard-coded parameters
    const float spec_power   = 32.0;   // shininess
    const float spec_strength= 0.6;    // spec scale
    const float F0           = 0.04;   // dieletric base reflectance

    vec4 base_color = texture(TextureBaseColor, Input.TexCoord);

    vec3 n  = normalize(Input.Normal);
    vec3 v  = normalize(camera.pos - Input.Position.xyz);
    vec3 l  = normalize(sky.sun_direction.xyz);

    vec3 h  = normalize(l + v);

    // half-Lambert diffuse
    vec3 diffuse = base_color.rgb * halfLambert(n, l);

    // Blinn-Phong specular
    float NdotH = max(dot(n, h), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;

    // Fresnel (Schlick)
    float F = fresnelSchlick(max(dot(v, h), 0.0), F0);

    // Sun light color
    vec3 sunColor = sky.sun_color.rgb * sky.sun_intensity;

    vec3 color = diffuse + spec * F;
    color *= sunColor;

    FragColor = vec4(color, base_color.a);
})";

    class MaterialTextureBlinnPhong : public Std3DMaterial
    {
    public:
        using Std3DMaterial::Std3DMaterial;
        ~MaterialTextureBlinnPhong() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            // Add inputs first so Std3D can decide how to compute normals, etc.
            vsc->AddInput(VAT_VEC2, VAN::TexCoord);
            vsc->AddInput(VAT_VEC3, VAN::Normal);

            if(!Std3DMaterial::CustomVertexShader(vsc))
                return false;

            vsc->AddOutput(SVT_VEC2, "TexCoord");
            vsc->AddOutput(SVT_VEC4, "Position");
            vsc->AddOutput(SVT_VEC3, "Normal");

            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            // Bind base color texture sampler
            mci->AddTextureSampler(ShaderStage::Fragment,
                                   DescriptorSetType::PerMaterial,
                                   SamplerType::Sampler2D,
                                   mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4, "FragColor");
            fsc->SetMain(fs_main);
            return true;
        }
    };
}

// Factory
MaterialCreateInfo *CreateTextureBlinnPhong(const VulkanDevAttr *dev_attr, const Material3DCreateConfig *cfg)
{
    MaterialTextureBlinnPhong m(cfg);
    return m.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
