#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/mtl/UBOCommon.h>
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    // TerrainGrid — 无 Position 顶点输入，只有 TransformID
    // VS 通过 gl_VertexID 生成网格坐标，texelFetch 采样高度/法线

    constexpr FixedVertexEntry TERRAIN_GRID_VERTEX[] = {
        { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
    };

#ifdef HGL_L2W_USE_SSBO
    constexpr DescriptorKind TERRAIN_GRID_L2W_KIND = DescriptorKind::SSBO;
#endif
#ifdef HGL_L2W_USE_UBO
    constexpr DescriptorKind TERRAIN_GRID_L2W_KIND = DescriptorKind::UBO;
#endif

    // Resort 字母序: camera=0, viewport=1 (Scene)
    //                TextureHeight=0, TextureNormal=1 (Material)
    constexpr FixedDescriptorEntry TERRAIN_GRID_DESCRIPTORS[] = {
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo",     nullptr },
        { DescriptorSetType::Scene,     DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",       nullptr },
        { DescriptorSetType::Transform, TERRAIN_GRID_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
        { DescriptorSetType::Material,  DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "TextureHeight", nullptr, "sampler2D" },
        { DescriptorSetType::Material,  DescriptorKind::Texture, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "TextureNormal", nullptr, "sampler2D" },
    };

    constexpr FixedMaterialDef TERRAIN_GRID_DEF {
        "TerrainGrid",
        PrimitiveType::Triangles,
        TERRAIN_GRID_VERTEX,
        uint32_t(sizeof(TERRAIN_GRID_VERTEX) / sizeof(TERRAIN_GRID_VERTEX[0])),
        TERRAIN_GRID_DESCRIPTORS,
        uint32_t(sizeof(TERRAIN_GRID_DESCRIPTORS) / sizeof(TERRAIN_GRID_DESCRIPTORS[0])),
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateTerrainGrid(const contract::PhysicalDeviceProfileLite *profile, const TerrainGridCreateConfig *cfg)
{
    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(
        SurfaceType::Terrain,
        BlendMode::Opaque,
        PassType::ForwardOpaque,
        QualityTier::Medium,
        PlatformBackend::PC,
        "compositor/main_terrain_grid.vert.glsl",
        "compositor/main_terrain_grid.frag.glsl",
        "surface/terrain_grid_surface.glsl"
    );

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
