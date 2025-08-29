#include "Std3DMaterial.h"
#include <hgl/graph/mtl/TerrainHeightmapMaterial.h>
#include <hgl/graph/mtl/BlinnPhong.h>
#include <hgl/graph/mtl/SamplerName.h>
#include <hgl/graph/mtl/ShaderVariableType.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include "common/MFGetPosition.h"

STD_MTL_NAMESPACE_BEGIN

namespace
{
    // Sampler names for terrain textures
    namespace TerrainSamplerName
    {
        constexpr const char Heightmap[] = "HeightmapTexture";
        constexpr const char BlendWeights[] = "BlendWeightsTexture";
        constexpr const char ColorArray[] = "ColorTextureArray";
        constexpr const char NormalArray[] = "NormalTextureArray";
    }

    // Material instance data for terrain parameters
    constexpr const char mi_codes[] = R"(
        float height_scale;
        float terrain_scale;
        vec2 padding;
    )";
    constexpr const uint32_t mi_bytes = sizeof(float) * 4; // height_scale, terrain_scale, padding

    // Vertex shader main function
    constexpr const char vs_main[] = R"(
void main()
{
    MaterialInstance mi = GetMI();
    
    // Convert uvec2 grid position to normalized UV coordinates
    vec2 uv = vec2(Position) * mi.terrain_scale;
    
    // Sample heightmap to get Z value
    float height = texture(HeightmapTexture, uv).r * mi.height_scale;
    
    // Construct 3D world position with sampled height
    vec3 worldPos = vec3(uv.x, height, uv.y);
    
    // Pass UV coordinates and world position to fragment shader
    Output.UV = uv;
    Output.WorldPos = worldPos;
    
    // Calculate vertex normal by sampling neighboring height values for normal computation
    vec2 texelSize = 1.0 / textureSize(HeightmapTexture, 0);
    float hL = texture(HeightmapTexture, uv + vec2(-texelSize.x, 0.0)).r * mi.height_scale;
    float hR = texture(HeightmapTexture, uv + vec2(texelSize.x, 0.0)).r * mi.height_scale;
    float hD = texture(HeightmapTexture, uv + vec2(0.0, -texelSize.y)).r * mi.height_scale;
    float hU = texture(HeightmapTexture, uv + vec2(0.0, texelSize.y)).r * mi.height_scale;
    
    // Compute normal using finite differences
    vec3 normal = normalize(vec3(hL - hR, 2.0 * mi.terrain_scale, hD - hU));
    Output.Normal = normal;
    
    // Calculate final vertex position for rendering
    gl_Position = camera.vp * vec4(worldPos, 1.0);
}
)";

    // Fragment shader with Blinn-Phong + Half Lambert lighting and 4-layer blending
    constexpr const char fs_main[] = R"(
void main()
{
    vec2 uv = Input.UV;
    vec3 worldPos = Input.WorldPos;
    vec3 normal = normalize(Input.Normal);
    
    // Sample blend weights (4 layers in RGBA channels)
    vec4 blendWeights = texture(BlendWeightsTexture, uv);
    
    // Normalize blend weights to ensure they sum to 1
    blendWeights = blendWeights / (blendWeights.r + blendWeights.g + blendWeights.b + blendWeights.a + 0.001);
    
    // Sample colors from texture array and blend
    vec3 color0 = texture(ColorTextureArray, vec3(uv, 0.0)).rgb;
    vec3 color1 = texture(ColorTextureArray, vec3(uv, 1.0)).rgb;
    vec3 color2 = texture(ColorTextureArray, vec3(uv, 2.0)).rgb;
    vec3 color3 = texture(ColorTextureArray, vec3(uv, 3.0)).rgb;
    
    vec3 finalColor = color0 * blendWeights.r + 
                     color1 * blendWeights.g + 
                     color2 * blendWeights.b + 
                     color3 * blendWeights.a;
    
    // Sample normals from texture array and blend
    vec3 normal0 = texture(NormalTextureArray, vec3(uv, 0.0)).rgb * 2.0 - 1.0;
    vec3 normal1 = texture(NormalTextureArray, vec3(uv, 1.0)).rgb * 2.0 - 1.0;
    vec3 normal2 = texture(NormalTextureArray, vec3(uv, 2.0)).rgb * 2.0 - 1.0;
    vec3 normal3 = texture(NormalTextureArray, vec3(uv, 3.0)).rgb * 2.0 - 1.0;
    
    vec3 finalNormal = normalize(normal0 * blendWeights.r + 
                                normal1 * blendWeights.g + 
                                normal2 * blendWeights.b + 
                                normal3 * blendWeights.a);
    
    // Combine surface normal with texture normal (simple approach)
    vec3 blendedNormal = normalize(normal + finalNormal);
    
    // Fixed directional sunlight
    vec3 lightDir = normalize(vec3(0.3, 0.8, 0.5)); // Fixed sun direction
    vec3 lightColor = vec3(1.0, 0.95, 0.8); // Warm sunlight color
    
    // Half Lambert diffuse lighting (softer lighting)
    float NdotL = dot(blendedNormal, lightDir);
    float halfLambert = NdotL * 0.5 + 0.5;
    
    // View direction for specular (assuming camera at origin for now)
    vec3 viewDir = normalize(-worldPos);
    
    // Blinn-Phong specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(blendedNormal, halfwayDir), 0.0);
    float specular = pow(NdotH, 32.0); // Fixed shininess for now
    
    // Combine lighting
    vec3 ambient = finalColor * 0.2;
    vec3 diffuse = finalColor * lightColor * halfLambert;
    vec3 specularColor = lightColor * specular * 0.5;
    
    vec3 result = ambient + diffuse + specularColor;
    
    FragColor = vec4(result, 1.0);
}
)";

    class TerrainHeightmapMaterial : public Std3DMaterial
    {
    public:
        using Std3DMaterial::Std3DMaterial;
        virtual ~TerrainHeightmapMaterial() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if (!Std3DMaterial::CustomVertexShader(vsc))
                return false;

            // Add texture samplers
            mci->AddSampler(VK_SHADER_STAGE_VERTEX_BIT, TerrainSamplerName::Heightmap);

            // Add outputs for fragment shader
            vsc->AddOutput(SVT_VEC2, "UV");
            vsc->AddOutput(SVT_VEC3, "WorldPos");
            vsc->AddOutput(SVT_VEC3, "Normal");

            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            // Add texture samplers for fragment shader
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, TerrainSamplerName::BlendWeights);
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, TerrainSamplerName::ColorArray);
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT, TerrainSamplerName::NormalArray);

            fsc->AddOutput(VAT_VEC4, "FragColor");

            fsc->SetMain(fs_main);
            return true;
        }

        bool EndCustomShader() override
        {
            // Set material instance data
            mci->SetMaterialInstance(mi_codes, 
                                   mi_bytes, 
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

            return true;
        }
    };
}

MaterialCreateInfo *CreateTerrainHeightmapMaterial(const VulkanDevAttr *dev_attr, const TerrainHeightmapMaterialCreateConfig *cfg)
{
    TerrainHeightmapMaterial terrain_material(cfg);
    return terrain_material.Create(dev_attr);
}

const AnsiString TerrainHeightmapMaterialCreateConfig::ToHashString()
{
    AnsiString result = Material3DCreateConfig::ToHashString();
    result += "_TerrainHeightmap";
    result += "_HS" + AnsiString(height_scale);
    result += "_TS" + AnsiString(terrain_scale);
    return result;
}

STD_MTL_NAMESPACE_END