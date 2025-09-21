#include "Std3DMaterial.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/UBOCommon.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // Vertex: generate grid from gl_VertexID, sample height and normal via texelFetch
    // Samplers (per-material): TextureHeight (R), TextureNormal (RGB)
    // Outputs: Position (clip space pos for FS), Normal (world space)

    constexpr const char vs_main[] = R"(
void main()
{
    // Get texture size
    ivec2 tex_sz = textureSize(TextureHeight, 0);
    int W = tex_sz.x;

    // Grid coordinate from vertex id
    int idx = gl_VertexID;
    ivec2 coord = ivec2(idx % W, idx / W);

    // Sample height (R) and normal (RGB)
    float h = texelFetch(TextureHeight, coord, 0).r;
    vec3 nrm = normalize(texelFetch(TextureNormal, coord, 0).xyz * 2.0 - 1.0);

    // Build local position: X = u, Y = v, Z = height
    vec3 pos = vec3(float(coord.x), float(coord.y), h);

    // Transform to world and clip space
    mat4 l2w = GetLocalToWorld();
    vec4 wp = l2w * vec4(pos, 1.0);

    // Transform normal to world (approx; ignore non-uniform scale)
    vec3 wn = normalize(mat3(l2w) * nrm);

    Output.Normal = wn;
    Output.Position = camera.vp * wp;

    gl_Position = Output.Position;
})";

    constexpr const char fs_main[] = R"(
const vec3 SUN_DIRECTION = normalize(vec3(0.655386, 0.491539, 0.573462));
const vec3 SUN_COLOR = vec3(1.0, 1.0, 1.0);
const vec3 BASE_COLOR = vec3(0.45, 0.6, 0.35); // hard-coded terrain color

void main()
{
    vec3 n = normalize(Input.Normal);

    // Diffuse (half-Lambert like)
    float intensity = 0.5 * max(dot(n, SUN_DIRECTION), 0.0) + 0.5;
    vec3 direct_color = intensity * SUN_COLOR * BASE_COLOR;

    // Simple Blinn-Phong specular using interpolated position as in other materials
    vec3 spec_color = vec3(0.0);
    if (intensity > 0.0)
    {
        vec3 half_vector = normalize(SUN_DIRECTION + normalize(Input.Position.xyz + camera.pos));
        float spec = max(dot(half_vector, n), 0.0);
        spec_color = spec * pow(spec, 16.0) * SUN_COLOR;
    }

    FragColor = vec4(direct_color + spec_color, 1.0);
})";

    class MaterialTerrainGrid : public Std3DMaterial
    {
    public:
        using Std3DMaterial::Std3DMaterial;
        ~MaterialTerrainGrid() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            // Setup required UBOs and instanced assign without adding a Position input
            mci->SetLocalToWorld((uint32_t)ShaderStage::AllGraphics);
            mci->AddUBOStruct((uint32_t)ShaderStage::AllGraphics, SBS_CameraInfo);

            // Instance assign provides GetLocalToWorld
            vsc->AddAssign();

            // Per-material samplers used in VS
            mci->AddTexture(ShaderStage::Vertex, DescriptorSetType::PerMaterial, TextureType::Texture2D, "TextureHeight");
            mci->AddTexture(ShaderStage::Vertex, DescriptorSetType::PerMaterial, TextureType::Texture2D, "TextureNormal");

            // Outputs to FS
            vsc->AddOutput(SVT_VEC4, "Position");
            vsc->AddOutput(SVT_VEC3, "Normal");

            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            // Camera UBO already added for AllGraphics
            fsc->AddOutput(VAT_VEC4, "FragColor");
            fsc->SetMain(fs_main);
            return true;
        }
    };
}

MaterialCreateInfo *CreateTerrainGrid(const VulkanDevAttr *dev_attr, const Material3DCreateConfig *cfg)
{
    MaterialTerrainGrid m(cfg);
    return m.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
