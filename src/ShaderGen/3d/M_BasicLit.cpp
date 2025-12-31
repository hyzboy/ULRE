#include "Std3DMaterial.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/UBOCommon.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char mi_codes[] = R"(
        uint base_color;
        float metallic;
        float roughness;
        float fresnel;
        float ibl_intensity;
    )";
    constexpr const uint32_t mi_bytes = sizeof(float) * 5;

    constexpr const char vs_main[] = R"(
void main()
{
    HandoverMI();
    Output.Normal   = GetNormal();
    Output.Position = GetPosition3D();
    gl_Position     = Output.Position;
})";

    constexpr const char fs_main[] = R"(
vec3 halfLambert(vec3 normal, vec3 lightDir)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

void main()
{
    MaterialInstance mi = GetMI();

    vec3 normal = normalize(Input.Normal);
    vec3 viewDir = normalize(camera.pos - Input.Position.xyz);
    vec3 lightDir = normalize(sky.sun_direction.xyz);

    vec4 base_color = unpackUnorm4x8(mi.base_color);

    // Half-Lambert diffuse
    vec3 diffuse = base_color.rgb * halfLambert(normal, lightDir);

    diffuse= max(diffuse, vec3(0.1)); // 保底漫反射，防止全黑

    // Blinn-Phong specular
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 32.0) * mi.metallic;

    // Fresnel
    float fresnel = fresnelSchlick(max(dot(viewDir, halfDir), 0.0), mi.fresnel);

    // Directional light color
    vec3 sunColor = sky.sun_color.rgb * sky.sun_intensity;

    sunColor = max(sunColor,vec3(0.1));

    // Combine
    vec3 color = diffuse + spec * fresnel;
    color *= sunColor;

#ifdef USE_IBL
    // 简单IBL: 直接加一份环境色
    color += mi.ibl_intensity * sky.base_sky_color.rgb;
#endif

    FragColor = vec4(color, 1.0);
})";

    class MaterialBasicLit : public Std3DMaterial
    {
        bool use_ibl;

    public:
        MaterialBasicLit(const Material3DCreateConfig *cfg, bool ibl)
            : Std3DMaterial(cfg), use_ibl(ibl) {}

        ~MaterialBasicLit() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            vsc->AddInput(VAT_VEC3, VAN::Normal);
            vsc->AddOutput(SVT_VEC4, "Position");
            vsc->AddOutput(SVT_VEC3, "Normal");
            if (!Std3DMaterial::CustomVertexShader(vsc))
                return false;
            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4, "FragColor");

            if(use_ibl)
                fsc->AddDefine("USE_IBL","true");

            fsc->SetMain(fs_main);
            return true;
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(mi_codes, mi_bytes, (uint32_t)ShaderStage::Fragment);
            return true;
        }
    };
}

MaterialCreateInfo *CreateBasicLit(const VulkanDevAttr *dev_attr, const BasicLitMaterialCreateConfig *cfg)
{
    MaterialBasicLit m(cfg, cfg->ibl);

    return m.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
