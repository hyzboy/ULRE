#pragma once

#include "GeometryFetchMode.h"

namespace hgl::graph
{
    struct DeviceQualityProfile
    {
        GeometryFetchMode   geometry_fetch;

        // 特性掩码
        bool support_ssbo_vertex;       // SSBO 顶点获取
        bool support_meshlet;           // Meshlet 管线
        bool support_hzb;               // HZB 生成和遮挡剔除
        bool support_clustered;         // Clustered Shading
        bool support_vbuffer;           // VBuffer 路径
        bool support_compute;           // Compute Shader
        bool support_indirect_draw;     // Indirect Draw
        bool support_d32_sfloat;        // D32_SFLOAT 深度格式

        uint32 max_texture_size;        // 最大纹理尺寸
        float  render_scale;            // 渲染分辨率缩放 (0.5~1.0)
        uint8  max_shadow_cascade;      // 最大阴影级联数
        uint8  max_point_lights;        // 最大点光源数

        // 默认构造 — PC High
        DeviceQualityProfile()
            : geometry_fetch(GeometryFetchMode::SSBO)
            , support_ssbo_vertex(true)
            , support_meshlet(false)
            , support_hzb(true)
            , support_clustered(true)
            , support_vbuffer(true)
            , support_compute(true)
            , support_indirect_draw(true)
            , support_d32_sfloat(true)
            , max_texture_size(4096)
            , render_scale(1.0f)
            , max_shadow_cascade(4)
            , max_point_lights(64)
        {}
    };

    class VulkanPhyDevice;

    DeviceQualityProfile DetectDeviceQuality(const VulkanPhyDevice &phy_device);
}
