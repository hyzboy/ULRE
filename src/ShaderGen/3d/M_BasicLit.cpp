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
        uint base_color;
        float metallic;
        float roughness;
        float fresnel;
        float ibl_intensity;
        float normal_strength;
    )";
    constexpr const uint32_t mi_bytes = sizeof(uint32_t) + sizeof(float) * 5;

    constexpr FixedVertexEntry BASIC_LIT_VERTEX[] = {
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
        { VAT_VEC2, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::TexCoord },
        { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Normal },
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
        { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::MaterialInstanceID::VIS_NAME },
    };

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
    constexpr DescriptorKind BASIC_LIT_L2W_KIND = DescriptorKind::SSBO;
#else
    constexpr DescriptorKind BASIC_LIT_L2W_KIND = DescriptorKind::UBO;
#endif

    constexpr FixedDescriptorEntry BASIC_LIT_DESCRIPTORS[] = {
        { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
        { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "sky", "SkyInfo", nullptr },
        { DescriptorSetType::PerFrame, BASIC_LIT_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureBaseColor", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureNormal", nullptr, "sampler2D" },
        { DescriptorSetType::PerMaterial, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), "TextureRoughness", nullptr, "sampler2D" },
    };

    constexpr const char vs_main[] = R"(
void main()
{
    HandoverMI();
    Output.TexCoord = TexCoord;
    Output.Normal   = GetNormal();
    Output.Position = GetPosition3D();
    gl_Position     = Output.Position;
})";

    constexpr const char fs_main[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
#define ULRE_SURFACE_TEX_MODE_COLOR_ONLY 1
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL 2
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS 3

#undef ULRE_SURFACE_TEX_MODE
#define ULRE_SURFACE_TEX_MODE ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS


vec3 halfLambert(vec3 normal, vec3 lightDir)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
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

    vec2 uv = ResolveSurfaceUV(Input.TexCoord);
    vec3 normal = ResolveSurfaceNormal(Input.Normal, uv, mi.normal_strength);
    vec3 viewDir = normalize(camera.pos - Input.Position.xyz);
    vec3 lightDir = normalize((camera.view * vec4(ULRE_GetSkyLightDir(), 0.0)).xyz);

    vec3 sampled_albedo = ResolveAlbedoColor(uv);
    vec4 base_color = unpackUnorm4x8(mi.base_color) * vec4(sampled_albedo, 1.0);

#if ULRE_SURFACE_TEX_MODE == ULRE_SURFACE_TEX_MODE_COLOR_ONLY
    FragColor = vec4(base_color.rgb, 1.0);
    return;
#endif

    float roughness_mix = ResolveSurfaceRoughness(mi.roughness, uv);

    // Half-Lambert diffuse
    vec3 diffuse = base_color.rgb * halfLambert(normal, lightDir);

    diffuse= max(diffuse, vec3(0.1)); // 保底漫反射，防止全黑

    // Blinn-Phong specular
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec_power = mix(96.0, 8.0, roughness_mix);
    float spec = pow(max(dot(normal, halfDir), 0.0), spec_power) * mi.metallic;

    // Fresnel
    float fresnel = fresnelSchlick(max(dot(viewDir, halfDir), 0.0), mi.fresnel);

    // Directional light color
    vec3 sunColor = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    // Combine
    vec3 color = diffuse + spec * fresnel;
    color *= sunColor;
    color += skyAmbient * 0.25;

#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    // 简单IBL: 直接加一份环境色
    color += mi.ibl_intensity * sky.base_sky_color.rgb;
#endif

    FragColor = vec4(color, 1.0);
})";

    constexpr const char BASIC_LIT_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.TexCoord = vi.TexCoord;
    Output.Normal = normalize(mat3(camera.view * GetLocalToWorld()) * vi.Normal);
    Output.Position = camera.vp * GetLocalToWorld() * vec4(vi.Position, 1.0);
    return vec4(vi.Position, 1.0);
}
)";

    constexpr const char BASIC_LIT_FS_BUSINESS[] = ULRE_SKYLIGHT_GLSL_COMMON R"(
#define ULRE_SURFACE_TEX_MODE_COLOR_ONLY 1
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL 2
#define ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS 3

#undef ULRE_SURFACE_TEX_MODE
#define ULRE_SURFACE_TEX_MODE ULRE_SURFACE_TEX_MODE_COLOR_NORMAL_ROUGHNESS


vec3 halfLambert(vec3 normal, vec3 lightDir)
{
    float NdotL = max(dot(normal, lightDir), 0.0);
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

    vec2 uv = ResolveSurfaceUV(Input.TexCoord);
    vec3 normal = ResolveSurfaceNormal(Input.Normal, uv, mi.normal_strength);
    vec3 viewDir = normalize(camera.pos - Input.Position.xyz);
    vec3 lightDir = normalize((camera.view * vec4(ULRE_GetSkyLightDir(), 0.0)).xyz);

    vec3 sampled_albedo = ResolveAlbedoColor(uv);
    vec4 base_color = unpackUnorm4x8(mi.base_color) * vec4(sampled_albedo, 1.0);

#if ULRE_SURFACE_TEX_MODE == ULRE_SURFACE_TEX_MODE_COLOR_ONLY
    return vec4(base_color.rgb, 1.0);
#endif

    float roughness_mix = ResolveSurfaceRoughness(mi.roughness, uv);

    vec3 diffuse = base_color.rgb * halfLambert(normal, lightDir);
    diffuse = max(diffuse, vec3(0.1));

    vec3 halfDir = normalize(lightDir + viewDir);
    float spec_power = mix(96.0, 8.0, roughness_mix);
    float spec = pow(max(dot(normal, halfDir), 0.0), spec_power) * mi.metallic;
    float fresnel = fresnelSchlick(max(dot(viewDir, halfDir), 0.0), mi.fresnel);

    vec3 sunColor = max(ULRE_GetSkyLightColor(), vec3(0.20));
    vec3 skyAmbient = ULRE_GetSkyAmbientColor();

    vec3 color = diffuse + spec * fresnel;
    color *= sunColor;
    color += skyAmbient * 0.25;

#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL
    color += mi.ibl_intensity * sky.base_sky_color.rgb;
#endif

    return vec4(color, 1.0);
}
)";

    constexpr VertexShaderBusiness BASIC_LIT_VERTEX_BUSINESS { BASIC_LIT_VS_BUSINESS };
    constexpr FragmentShaderBusiness BASIC_LIT_FRAGMENT_BUSINESS { BASIC_LIT_FS_BUSINESS };

    constexpr FixedMaterialDef BASIC_LIT_DEF {
        "BasicLit_v2",
        PrimitiveType::Triangles,
        BASIC_LIT_VERTEX,
        uint32_t(sizeof(BASIC_LIT_VERTEX) / sizeof(BASIC_LIT_VERTEX[0])),
        BASIC_LIT_DESCRIPTORS,
        uint32_t(sizeof(BASIC_LIT_DESCRIPTORS) / sizeof(BASIC_LIT_DESCRIPTORS[0])),
        mi_codes,
        mi_bytes,
        vs_main,
        nullptr,
        fs_main,
    };

    const ComposedMaterialDef BASIC_LIT_COMPOSED_DEF {
        "BasicLit_v2",
        PrimitiveType::Triangles,
        BASIC_LIT_VERTEX,
        uint32_t(sizeof(BASIC_LIT_VERTEX) / sizeof(BASIC_LIT_VERTEX[0])),
        BASIC_LIT_DESCRIPTORS,
        uint32_t(sizeof(BASIC_LIT_DESCRIPTORS) / sizeof(BASIC_LIT_DESCRIPTORS[0])),
        &BASIC_LIT_VERTEX_BUSINESS,
        &BASIC_LIT_FRAGMENT_BUSINESS,
        ShaderOutputMode::SingleRTAlphaBlend,
        false,
        mi_codes,
        mi_bytes,
    };

    constexpr const char* BASIC_LIT_VERTEX_RESOURCES[] = {
        "camera",
        "l2w"
    };

    constexpr const char* BASIC_LIT_FRAGMENT_RESOURCES[] = {
        "camera",
        "sky",
        "mtl",
        "TextureBaseColor",
        "TextureNormal",
        "TextureRoughness"
    };

    constexpr const char* BASIC_LIT_FRAGMENT_HELPERS[] = {
        "GetMI"
    };

    const VertexShaderLogic BASIC_LIT_VERTEX_SHADER_LOGIC = {
        {
            BASIC_LIT_VS_BUSINESS,
            nullptr,
            BASIC_LIT_VERTEX_RESOURCES,
            2,
            nullptr,
            0
        }
    };

    const FragmentShaderLogic BASIC_LIT_FRAGMENT_SHADER_LOGIC = {
        {
            BASIC_LIT_FS_BUSINESS,
            nullptr,
            BASIC_LIT_FRAGMENT_RESOURCES,
            6,
            BASIC_LIT_FRAGMENT_HELPERS,
            1
        }
    };

    const MaterialLogicDef BASIC_LIT_LOGIC = {
        BASIC_LIT_VERTEX_SHADER_LOGIC,
        BASIC_LIT_FRAGMENT_SHADER_LOGIC,
        nullptr,
        nullptr,
        nullptr
    };

    class MaterialBasicLit : public Std3DMaterial
    {
        bool use_ibl;

    public:
        MaterialBasicLit(const Material3DCreateConfig *cfg, bool ibl)
            : Std3DMaterial(cfg), use_ibl(ibl) {}

        ~MaterialBasicLit() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            vsc->AddInput(VAT_VEC2, VAN::TexCoord);
            vsc->AddInput(VAT_VEC3, VAN::Normal);
            vsc->AddOutput(SVT_VEC2, "TexCoord");
            vsc->AddOutput(SVT_VEC4, "Position");
            vsc->AddOutput(SVT_VEC3, "Normal");
            if (!Std3DMaterial::CustomVertexShader(vsc))
                return false;
            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
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

            if(use_ibl)
                fsc->AddDefine("ULRE_SKYLIGHT_MODEL","ULRE_SKYLIGHT_MODEL_IBL");

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

MaterialCreateInfo *CreateBasicLit(const VulkanDevAttr *dev_attr, BasicLitMaterialCreateConfig *cfg)
{
    if(cfg)
        cfg->material_instance=true;

    // 先保持 IBL 分支走 legacy，避免与当前 key/define 系统耦合
    if (cfg && cfg->ibl)
    {
        std::fprintf(stderr,
            "[BasicLit] IBL=true, using legacy Std3DMaterial path\n");

        MaterialBasicLit m(cfg, cfg->ibl);
        return m.Create(dev_attr);
    }

    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        BASIC_LIT_DEF,
        BASIC_LIT_COMPOSED_DEF,
        BASIC_LIT_LOGIC,
        key,
        cfg);

    if (mci_new)
    {
        std::fprintf(stderr,
            "[BasicLit] using new composed-business compile path\n");
        return mci_new;
    }

    std::fprintf(stderr,
        "[BasicLit] composed-business compile failed, fallback to legacy Std3DMaterial path\n");

    MaterialBasicLit m(cfg, cfg ? cfg->ibl : false);
    return m.Create(dev_attr);
}
}//namespace hgl::graph::mtl
