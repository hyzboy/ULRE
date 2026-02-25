#include "Std3DMaterial.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/graph/mtl/MaterialCompiler.h>
#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>
#include <cstdio>

#include "common/MFSkyLight.h"

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry TEXTURE_BLINN_PHONG_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
    };

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
    constexpr DescriptorKind TEXTURE_BLINN_PHONG_L2W_KIND = DescriptorKind::SSBO;
#else
    constexpr DescriptorKind TEXTURE_BLINN_PHONG_L2W_KIND = DescriptorKind::UBO;
#endif

    constexpr FixedDescriptorEntry TEXTURE_BLINN_PHONG_DESCRIPTORS[] = {
        { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::PerFrame, TEXTURE_BLINN_PHONG_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
    };

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
    constexpr const char fs_main[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
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
    vec3 l  = ULRE_GetSkyLightDir();

    vec3 h  = normalize(l + v);

    // half-Lambert diffuse
    vec3 diffuse = base_color.rgb * halfLambert(n, l);

    // Blinn-Phong specular
    float NdotH = max(dot(n, h), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;

    // Fresnel (Schlick)
    float F = fresnelSchlick(max(dot(v, h), 0.0), F0);

    // Sun light color
    vec3 sunColor = ULRE_GetSkyLightColor();
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * F;
    color *= sunColor;
    color += skyAmbient * 0.15;

    FragColor = vec4(color, base_color.a);
})";

    constexpr const char TEXTURE_BLINN_PHONG_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.TexCoord = vi.TexCoord;
    Output.Normal = normalize(mat3(camera.view * GetLocalToWorld()) * vi.Normal);
    Output.Position = camera.vp * GetLocalToWorld() * vec4(vi.Position, 1.0);
    return vec4(vi.Position, 1.0);
}
)";

    constexpr const char TEXTURE_BLINN_PHONG_FS_BUSINESS[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
vec3 halfLambert(vec3 n, vec3 l)
{
    float NdotL = max(dot(n, l), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec4 FragmentShaderBusiness()
{
    const float spec_power   = 32.0;
    const float spec_strength= 0.6;
    const float F0           = 0.04;

    vec4 base_color = texture(TextureBaseColor, Input.TexCoord);

    vec3 n  = normalize(Input.Normal);
    vec3 v  = normalize(camera.pos - Input.Position.xyz);
    vec3 l  = ULRE_GetSkyLightDir();

    vec3 h  = normalize(l + v);

    vec3 diffuse = base_color.rgb * halfLambert(n, l);

    float NdotH = max(dot(n, h), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;

    float F = fresnelSchlick(max(dot(v, h), 0.0), F0);

    vec3 sunColor = ULRE_GetSkyLightColor();
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * F;
    color *= sunColor;
    color += skyAmbient * 0.15;

    return vec4(color, base_color.a);
}
)";

    constexpr VertexShaderBusiness TEXTURE_BLINN_PHONG_VERTEX_BUSINESS { TEXTURE_BLINN_PHONG_VS_BUSINESS };
    constexpr FragmentShaderBusiness TEXTURE_BLINN_PHONG_FRAGMENT_BUSINESS { TEXTURE_BLINN_PHONG_FS_BUSINESS };

    constexpr FixedMaterialDef TEXTURE_BLINN_PHONG_DEF {
        "TextureBlinnPhong",
        PrimitiveType::Triangles,
        TEXTURE_BLINN_PHONG_VERTEX,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_VERTEX) / sizeof(TEXTURE_BLINN_PHONG_VERTEX[0])),
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])),
        nullptr,
        0,
        vs_main,
        nullptr,
        fs_main,
    };

    const ComposedMaterialDef TEXTURE_BLINN_PHONG_COMPOSED_DEF {
        "TextureBlinnPhong",
        PrimitiveType::Triangles,
        TEXTURE_BLINN_PHONG_VERTEX,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_VERTEX) / sizeof(TEXTURE_BLINN_PHONG_VERTEX[0])),
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])),
        &TEXTURE_BLINN_PHONG_VERTEX_BUSINESS,
        &TEXTURE_BLINN_PHONG_FRAGMENT_BUSINESS,
        ShaderOutputMode::SingleRTAlphaBlend,
        false,
        nullptr,
        0,
    };

    constexpr const char* TEXTURE_BLINN_PHONG_VERTEX_RESOURCES[] = {
        "camera",
        "l2w"
    };

    constexpr const char* TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES[] = {
        "camera",
        "sky",
        "TextureBaseColor"
    };

    const VertexShaderLogic TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC = {
        {
            TEXTURE_BLINN_PHONG_VS_BUSINESS,
            nullptr,
            TEXTURE_BLINN_PHONG_VERTEX_RESOURCES,
            2,
            nullptr,
            0
        }
    };

    const FragmentShaderLogic TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC = {
        {
            TEXTURE_BLINN_PHONG_FS_BUSINESS,
            nullptr,
            TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES,
            3,
            nullptr,
            0
        }
    };

    const MaterialLogicDef TEXTURE_BLINN_PHONG_LOGIC = {
        TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC,
        TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC,
        nullptr,
        nullptr,
        nullptr
    };

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
    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        TEXTURE_BLINN_PHONG_DEF,
        TEXTURE_BLINN_PHONG_COMPOSED_DEF,
        TEXTURE_BLINN_PHONG_LOGIC,
        key,
        cfg);

    if (mci_new)
    {
        std::fprintf(stderr,
            "[TextureBlinnPhong] using new composed-business compile path\n");
        return mci_new;
    }

    std::fprintf(stderr,
        "[TextureBlinnPhong] composed-business compile failed, fallback to legacy Std3DMaterial path\n");

    MaterialTextureBlinnPhong m(cfg);
    return m.Create(dev_attr);
}
}//namespace hgl::graph::mtl
