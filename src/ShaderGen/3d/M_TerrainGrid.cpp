#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include <hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/UBOCommon.h>
#include <memory>

namespace hgl::graph::mtl{
namespace
{
    constexpr const FixedVertexEntry *TERRAIN_GRID_VERTEX_PTR = nullptr;
    constexpr uint32_t TERRAIN_GRID_VERTEX_COUNT = 0;

    constexpr SamplerSlot TERRAIN_GRID_TEX_SLOTS[] = {
        SamplerSlot::Height,
        SamplerSlot::Normal,
    };

    const UBOSemanticSet TERRAIN_GRID_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet TERRAIN_GRID_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticTextureSamplerDescriptors TERRAIN_GRID_SAMPLERS = []
    {
        StaticTextureSamplerDescriptors descriptors;
        AddTextureSampler(descriptors, TERRAIN_GRID_TEX_SLOTS[0], SamplerType::Sampler2D);
        AddTextureSampler(descriptors, TERRAIN_GRID_TEX_SLOTS[1], SamplerType::Sampler2D);
        return descriptors;
    }();

    const StaticMaterialDef TERRAIN_GRID_DEF {
        "TerrainGrid",
        PrimitiveType::Triangles,
        TERRAIN_GRID_VERTEX_PTR,
        TERRAIN_GRID_VERTEX_COUNT,
        &TERRAIN_GRID_UBOS,
        &TERRAIN_GRID_SSBOS,
        &TERRAIN_GRID_SAMPLERS,
        ShaderDataSchema::None
    };
}//namespace

std::unique_ptr<MaterialCreateInfo> CreateTerrainGridOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                           const TerrainGridCreateConfig *cfg,
                                                           const MaterialVariantDesc &desc,
                                                           const MaterialVariantKey &key)
{
    return CreateFromFixedDef3DOwned("TerrainGrid", profile, TERRAIN_GRID_DEF, key, cfg, desc);
}
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY_FROM_OWNED(
    TerrainGrid,
    const hgl::graph::mtl::TerrainGridCreateConfig)
