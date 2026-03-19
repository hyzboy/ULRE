#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/mtl/UBOCommon.h>
#include<cstdio>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl{
namespace
{
    // TerrainGrid has no vertex attributes.
    // VS 通过 gl_VertexID 生成网格坐标，texelFetch 采样高度/法线

    constexpr const FixedVertexEntry *TERRAIN_GRID_VERTEX_PTR = nullptr;
    constexpr uint32_t TERRAIN_GRID_VERTEX_COUNT = 0;

    // Resort 字母�? camera=0, viewport=1 (Scene)
    //                TextureHeight=0, TextureNormal=1 (Material)
    constexpr FixedDescriptorEntry TERRAIN_GRID_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",     nullptr },
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",       nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "tid", "TransformIDData", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "TextureHeight", nullptr, "sampler2D" },
        { DescriptorSetType::Material,  DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "TextureNormal", nullptr, "sampler2D" },
    };

    constexpr FixedMaterialDef TERRAIN_GRID_DEF {
        "TerrainGrid",
        PrimitiveType::Triangles,
        TERRAIN_GRID_VERTEX_PTR,
        TERRAIN_GRID_VERTEX_COUNT,
        TERRAIN_GRID_DESCRIPTORS,
        uint32_t(sizeof(TERRAIN_GRID_DESCRIPTORS) / sizeof(TERRAIN_GRID_DESCRIPTORS[0])),
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateTerrainGrid(const contract::PhysicalDeviceProfileLite *profile, const TerrainGridCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    var_key.surface_type = SurfaceType::Terrain;
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[TerrainGrid] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, PlatformBackend::PC, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[TerrainGrid] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        TERRAIN_GRID_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[TerrainGrid] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
