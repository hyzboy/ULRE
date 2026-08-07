#pragma once

#include <cstdint>

namespace hgl::graph::DSBinding
{
    // Set 0 — PerScene (Resort 字母序: camera < sky < viewport, LightBuffer 末尾)
    namespace PerScene
    {
        constexpr uint32_t CameraInfo     = 0;    // CAMERA_BINDING
        constexpr uint32_t SkyInfo        = 1;    // SKY_BINDING
        constexpr uint32_t ViewportInfo   = 2;    // VIEWPORT_BINDING
        constexpr uint32_t LightBuffer    = 3;    // Lit 专用 SSBO
    }

    // Set 1 — PerView
    namespace PerView
    {
        constexpr uint32_t LocalToWorld   = 0;    // SSBO: mat4 池
    }

    // Set 2 — PerMaterial
    namespace PerMaterial
    {
        constexpr uint32_t MI_SSBO        = 0;
        constexpr uint32_t TexAlbedo      = 1;
        constexpr uint32_t TexNormal      = 2;
        constexpr uint32_t TexMR          = 3;    // Metallic-Roughness
        constexpr uint32_t TexAO          = 4;
        constexpr uint32_t TexEmissive    = 5;
        constexpr uint32_t TexDetail      = 6;    // Detail Normal
        // 7-12: Special Surface 扩展纹理
        constexpr uint32_t TexSpecial0    = 7;
        constexpr uint32_t TexSpecial1    = 8;
        constexpr uint32_t TexSpecial2    = 9;
        constexpr uint32_t TexSpecial3    = 10;
        constexpr uint32_t TexSpecial4    = 11;
        constexpr uint32_t TexSpecial5    = 12;
    }

    // Set 3 — PerDraw (Environment / Pipeline RT)
    namespace PerDraw
    {
        constexpr uint32_t ColorPalette       = 0;
        constexpr uint32_t ShadowMapNear      = 1;
        constexpr uint32_t ShadowMask         = 2;
        constexpr uint32_t SSAO_RT            = 3;
        constexpr uint32_t IBL_Irradiance     = 4;
        constexpr uint32_t IBL_Prefiltered    = 5;
        constexpr uint32_t IBL_BRDF_LUT      = 6;
        constexpr uint32_t SSS_LUT           = 7;
        constexpr uint32_t DebugLightingCfg   = 8;
        constexpr uint32_t HZB_Pyramid       = 9;
        constexpr uint32_t ClusterLightList   = 10;
        constexpr uint32_t ClusterAABB        = 11;
        constexpr uint32_t FogParams          = 12;
        constexpr uint32_t SSR_RT             = 13;
        constexpr uint32_t ExposureData       = 14;
        constexpr uint32_t MeshletBuffer      = 15;
        constexpr uint32_t InstanceBuffer     = 16;
        constexpr uint32_t TerrainHeightMap   = 17;
        constexpr uint32_t ShadowMapCached    = 20;
        constexpr uint32_t CapsuleShadowData  = 21;
    }
}
