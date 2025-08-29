#pragma once

#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/BlinnPhong.h>

STD_MTL_NAMESPACE_BEGIN

/**
 * Terrain heightmap material configuration
 * Supports heightmap-based terrain rendering with multi-layer texture blending
 */
struct TerrainHeightmapMaterialCreateConfig : public Material3DCreateConfig
{
    float height_scale = 1.0f;     ///< Scale factor for height values from heightmap
    float terrain_scale = 1.0f;    ///< Scale factor for terrain UV coordinates
    
public:

    TerrainHeightmapMaterialCreateConfig(const WithCamera &wc = WithCamera::With,
                                       const WithLocalToWorld &l2w = WithLocalToWorld::With)
        : Material3DCreateConfig(PrimitiveType::Triangles, wc, l2w)
    {
        // Terrain uses uvec2 grid positions as input
        position_format = VAT_UVEC2;
    }

    const int compare(const TerrainHeightmapMaterialCreateConfig &cfg) const override
    {
        int off = Material3DCreateConfig::compare(cfg);
        if (off) return off;

        if (height_scale < cfg.height_scale) return -1;
        if (height_scale > cfg.height_scale) return 1;

        if (terrain_scale < cfg.terrain_scale) return -1;
        if (terrain_scale > cfg.terrain_scale) return 1;

        return 0;
    }

    const AnsiString ToHashString() override;
};

/**
 * Create terrain heightmap material
 * @param dev_attr Vulkan device attributes
 * @param cfg Terrain material configuration
 * @return Material creation info
 */
MaterialCreateInfo *CreateTerrainHeightmapMaterial(const VulkanDevAttr *dev_attr, const TerrainHeightmapMaterialCreateConfig *cfg);

STD_MTL_NAMESPACE_END