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
    constexpr const char mi_codes[] = R"(
        float normal_strength;
    )";
    constexpr const uint32_t mi_bytes = sizeof(float);

    constexpr FixedVertexEntry TEXTURE_BLINN_PHONG_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::MaterialInstanceID::VIS_NAME },
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
        { DescriptorSetType::PerMaterial, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

    // Vertex: pass through position, compute normal, forward TexCoord
    constexpr const char vs_main[] = R"(
void main()
{
    HandoverMI();
    Output.TexCoord = TexCoord;
    Output.Normal   = GetNormal();
    Output.Position = GetPosition3D();
    gl_Position     = Output.Position;
})";

    // Fragment: textured Blinn-Phong + half-Lambert + Fresnel, lit by sky sun
    constexpr const char fs_main[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
#define ULRE_SURFACE_TEX_MODE_COLOR_ONLY 1
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL 2
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS 3

#undef ULRE_SURFACE_TEX_MODE
#define ULRE_SURFACE_TEX_MODE ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS


vec3 halfLambert(vec3 n, vec3 l)
{
    float NdotL = max(dot(n, l), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec2 ResolveSurfaceUV(vec2 uv)
{
    if (abs(uv.x) + abs(uv.y) < 0.0001)
        return vec2(0.5, 0.5);

    return fract(abs(uv));
}

vec3 ResolveAlbedoColor(vec2 uv)
{
    vec4 c = texture(TextureBaseColor, uv);
    vec3 rgb = c.rgb;

    if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
    {
        vec4 center = texture(TextureBaseColor, vec2(0.5, 0.5));
        rgb = center.rgb;

        if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
            rgb = vec3(max(c.a, center.a));
    }

    return rgb;
}

vec3 ResolveSurfaceNormal(vec3 input_normal, vec2 uv, float normal_strength)
{
#if ULRE_SURFACE_TEX_MODE >= ULRE_SURFACE_TEX_MODE_COLOR_NORMAL
    vec3 sampled_normal = texture(TextureNormal, uv).xyz * 2.0 - 1.0;
    sampled_normal.y = -sampled_normal.y;
    return normalize(input_normal + vec3(sampled_normal.xy, 0.0) * normal_strength);
#else
    return normalize(input_normal);
#endif
}

float ResolveSurfaceRoughness(float base_roughness, vec2 uv)
{
#if ULRE_SURFACE_TEX_MODE >= ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS
    float roughness_tex = texture(TextureRoughness, uv).r;
    return clamp(base_roughness * roughness_tex, 0.04, 1.0);
#else
    return clamp(base_roughness, 0.04, 1.0);
#endif
}

void main()
{
    MaterialInstance mi = GetMI();

    // Hard-coded parameters
    const float spec_strength= 0.6;    // spec scale
    const float F0           = 0.04;   // dieletric base reflectance

    vec2 uv = ResolveSurfaceUV(Input.TexCoord);
    vec4 base_color = vec4(ResolveAlbedoColor(uv), 1.0);

#if ULRE_SURFACE_TEX_MODE == ULRE_SURFACE_TEX_MODE_COLOR_ONLY
    FragColor = vec4(base_color.rgb, 1.0);
    return;
#endif

    float roughness = ResolveSurfaceRoughness(0.8, uv);
    float spec_power = mix(96.0, 8.0, roughness);

    vec3 n  = ResolveSurfaceNormal(Input.Normal, uv, mi.normal_strength);
    vec3 v  = vec3(0.0, 0.0, 1.0);
    vec3 l  = normalize((camera.view * vec4(ULRE_GetSkyLightDir(), 0.0)).xyz);

    vec3 h  = normalize(l + v);

    // half-Lambert diffuse
    vec3 diffuse = base_color.rgb * halfLambert(n, l);

    // Blinn-Phong specular
    float NdotH = max(dot(n, h), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;

    // Fresnel (Schlick)
    float F = fresnelSchlick(max(dot(v, h), 0.0), F0);

    // Sun light color
    vec3 sunColor = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * F;
    color *= sunColor;

#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    color += sky.base_sky_color.rgb * 0.15;
#else
    color += skyAmbient * 0.25;
#endif

    FragColor = vec4(color, 1.0);
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
#define ULRE_SURFACE_TEX_MODE_COLOR_ONLY 1
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL 2
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS 3

#undef ULRE_SURFACE_TEX_MODE
#define ULRE_SURFACE_TEX_MODE ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS


vec3 halfLambert(vec3 n, vec3 l)
{
    float NdotL = max(dot(n, l), 0.0);
    return vec3(NdotL * 0.5 + 0.5);
}

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec2 ResolveSurfaceUV(vec2 uv)
{
    if (abs(uv.x) + abs(uv.y) < 0.0001)
        return vec2(0.5, 0.5);

    return fract(abs(uv));
}

vec3 ResolveAlbedoColor(vec2 uv)
{
    vec4 c = texture(TextureBaseColor, uv);
    vec3 rgb = c.rgb;

    if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
    {
        vec4 center = texture(TextureBaseColor, vec2(0.5, 0.5));
        rgb = center.rgb;

        if (max(max(rgb.r, rgb.g), rgb.b) < 0.0001)
            rgb = vec3(max(c.a, center.a));
    }

    return rgb;
}

vec3 ResolveSurfaceNormal(vec3 input_normal, vec2 uv, float normal_strength)
{
#if ULRE_SURFACE_TEX_MODE >= ULRE_SURFACE_TEX_MODE_COLOR_NORMAL
    vec3 sampled_normal = texture(TextureNormal, uv).xyz * 2.0 - 1.0;
    sampled_normal.y = -sampled_normal.y;
    return normalize(input_normal + vec3(sampled_normal.xy, 0.0) * normal_strength);
#else
    return normalize(input_normal);
#endif
}

float ResolveSurfaceRoughness(float base_roughness, vec2 uv)
{
#if ULRE_SURFACE_TEX_MODE >= ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS
    float roughness_tex = texture(TextureRoughness, uv).r;
    return clamp(base_roughness * roughness_tex, 0.04, 1.0);
#else
    return clamp(base_roughness, 0.04, 1.0);
#endif
}

vec4 FragmentShaderBusiness()
{
    MaterialInstance mi = GetMI();

    const float spec_strength= 0.6;
    const float F0           = 0.04;

    vec2 uv = ResolveSurfaceUV(Input.TexCoord);
    vec4 base_color = vec4(ResolveAlbedoColor(uv), 1.0);

#if ULRE_SURFACE_TEX_MODE == ULRE_SURFACE_TEX_MODE_COLOR_ONLY
    return vec4(base_color.rgb, 1.0);
#endif

    float roughness = ResolveSurfaceRoughness(0.8, uv);
    float spec_power = mix(96.0, 8.0, roughness);

    vec3 n  = ResolveSurfaceNormal(Input.Normal, uv, mi.normal_strength);
    vec3 v  = vec3(0.0, 0.0, 1.0);
    vec3 l  = normalize((camera.view * vec4(ULRE_GetSkyLightDir(), 0.0)).xyz);

    vec3 h  = normalize(l + v);

    vec3 diffuse = base_color.rgb * halfLambert(n, l);

    float NdotH = max(dot(n, h), 0.0);
    float spec  = pow(NdotH, spec_power) * spec_strength;

    float F = fresnelSchlick(max(dot(v, h), 0.0), F0);

    vec3 sunColor = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * F;
    color *= sunColor;

#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    color += sky.base_sky_color.rgb * 0.15;
#else
    color += skyAmbient * 0.25;
#endif

    return vec4(color, 1.0);
}
)";

    constexpr VertexShaderBusiness TEXTURE_BLINN_PHONG_VERTEX_BUSINESS { TEXTURE_BLINN_PHONG_VS_BUSINESS };
    constexpr FragmentShaderBusiness TEXTURE_BLINN_PHONG_FRAGMENT_BUSINESS { TEXTURE_BLINN_PHONG_FS_BUSINESS };

    constexpr FixedMaterialDef TEXTURE_BLINN_PHONG_DEF {
        "TextureBlinnPhong_v2",
        PrimitiveType::Triangles,
        TEXTURE_BLINN_PHONG_VERTEX,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_VERTEX) / sizeof(TEXTURE_BLINN_PHONG_VERTEX[0])),
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
        vs_main,
        nullptr,
        fs_main,
    };

    const ComposedMaterialDef TEXTURE_BLINN_PHONG_COMPOSED_DEF {
        "TextureBlinnPhong_v2",
        PrimitiveType::Triangles,
        TEXTURE_BLINN_PHONG_VERTEX,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_VERTEX) / sizeof(TEXTURE_BLINN_PHONG_VERTEX[0])),
        TEXTURE_BLINN_PHONG_DESCRIPTORS,
        uint32_t(sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS) / sizeof(TEXTURE_BLINN_PHONG_DESCRIPTORS[0])),
        &TEXTURE_BLINN_PHONG_VERTEX_BUSINESS,
        &TEXTURE_BLINN_PHONG_FRAGMENT_BUSINESS,
        ShaderOutputMode::SingleRTAlphaBlend,
        false,
        mi_codes,
        mi_bytes,
    };

    constexpr const char* TEXTURE_BLINN_PHONG_VERTEX_RESOURCES[] = {
        "camera",
        "l2w"
    };

    constexpr const char* TEXTURE_BLINN_PHONG_FRAGMENT_RESOURCES[] = {
        "camera",
        "sky",
        "mtl",
        "TextureBaseColor",
        "TextureNormal",
        "TextureRoughness"
    };

    constexpr const char* TEXTURE_BLINN_PHONG_FRAGMENT_HELPERS[] = {
        "GetMI"
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
            6,
            TEXTURE_BLINN_PHONG_FRAGMENT_HELPERS,
            1
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
            mci->AddTextureSampler(ShaderStage::Fragment,
                                   DescriptorSetType::PerMaterial,
                                   SamplerType::Sampler2D,
                                   "TextureNormal");
            mci->AddTextureSampler(ShaderStage::Fragment,
                                   DescriptorSetType::PerMaterial,
                                   SamplerType::Sampler2D,
                                   "TextureRoughness");

            fsc->AddOutput(VAT_VEC4, "FragColor");
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

// Factory
MaterialCreateInfo *CreateTextureBlinnPhong(const VulkanDevAttr *dev_attr, const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig cfg_with_mi = cfg ? *cfg : Material3DCreateConfig();
    cfg_with_mi.material_instance = true;

    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        TEXTURE_BLINN_PHONG_DEF,
        TEXTURE_BLINN_PHONG_COMPOSED_DEF,
        TEXTURE_BLINN_PHONG_LOGIC,
        key,
        &cfg_with_mi);

    if (mci_new)
    {
        std::fprintf(stderr,
            "[TextureBlinnPhong] using new composed-business compile path\n");
        return mci_new;
    }

    std::fprintf(stderr,
        "[TextureBlinnPhong] composed-business compile failed, fallback to legacy Std3DMaterial path\n");

    MaterialTextureBlinnPhong m(&cfg_with_mi);
    return m.Create(dev_attr);
}
}//namespace hgl::graph::mtl
