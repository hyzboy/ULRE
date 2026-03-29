#include"FixedDefFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/UBOCommon.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const FixedVertexEntry *TERRAIN_GRID_VERTEX_PTR = nullptr;
    constexpr uint32_t TERRAIN_GRID_VERTEX_COUNT = 0;

    constexpr SamplerSlot TERRAIN_GRID_TEX_SLOTS[] = {
        SamplerSlot::Height,
        SamplerSlot::Normal,
    };

    const FixedUBODescriptors TERRAIN_GRID_UBOS = build3d::MakeViewportCameraUBOs();

    const FixedSSBODescriptors TERRAIN_GRID_SSBOS = build3d::MakeTransformSSBOs(false);

    const FixedTextureSamplerDescriptors TERRAIN_GRID_SAMPLERS = []
    {
        FixedTextureSamplerDescriptors descriptors;
        AddFixedTextureSampler(descriptors, TERRAIN_GRID_TEX_SLOTS[0], SamplerType::Sampler2D);
        AddFixedTextureSampler(descriptors, TERRAIN_GRID_TEX_SLOTS[1], SamplerType::Sampler2D);
        return descriptors;
    }();

    const FixedMaterialDef TERRAIN_GRID_DEF {
        "TerrainGrid",
        PrimitiveType::Triangles,
        TERRAIN_GRID_VERTEX_PTR,
        TERRAIN_GRID_VERTEX_COUNT,
        &TERRAIN_GRID_UBOS,
        &TERRAIN_GRID_SSBOS,
        &TERRAIN_GRID_SAMPLERS,
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateTerrainGrid(const contract::PhysicalDeviceProfileLite *profile, const TerrainGridCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithSurface(SurfaceType::Terrain);
    return CreateFromFixedDef3D("TerrainGrid", profile, TERRAIN_GRID_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
