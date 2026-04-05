#pragma once

/// MaterialAssetLoader.h — 从 MaterialAssetRecord 驱动材质创建的内联辅助函数
///
/// 将 MaterialAssetRecord 中的平铺字段还原为对应的
/// Material2DCreateConfig / Material3DCreateConfig / BillboardMaterialCreateConfig，
/// 调用 MaterialManager::AcquireMaterial，并可选地加载纹理并绑定到材质。
///
/// 用法示例（示例程序顶部的静态配置表）：
///   static const mtl::MaterialAssetRecord kMeshMtl {
///       .id      = "my_mesh",
///       .preset  = mtl::MaterialPreset::Standard,
///       .sky     = true,
///       .lighting = mtl::LightingModel::BlinnPhong,
///       .tex_base_color = "res/image/Brick/Albedo.Tex2D",
///       .tex_normal     = "res/image/Brick/Normal.Tex2D",
///   };
#include <hgl/mtl/MaterialAssetRecord.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/type/String.h>
#include <hgl/type/StdString.h>

namespace hgl::graph
{

// ── Material 创建（不含纹理绑定）──────────────────────────────────────────────

/// 从 MaterialAssetRecord 创建/获取 Material（仅 AcquireMaterial，不绑纹理）。
inline Material *CreateMaterialFromRecord(
    MaterialManager *mm,
    const mtl::MaterialAssetRecord &rec)
{
    if (!mm) return nullptr;

    using namespace mtl;

    // ── Billboard2DFixed / Billboard2DDynamic ────────────────────────────────
    if (rec.preset == MaterialPreset::Billboard2DFixed ||
        rec.preset == MaterialPreset::Billboard2DDynamic)
    {
        BillboardMaterialCreateConfig cfg(rec.prim);
        cfg.local_to_world  = rec.l2w;
        cfg.fixed_size      = rec.billboard.fixed_size;
        cfg.pixel_size      = { rec.billboard.pixel_w, rec.billboard.pixel_h };
        cfg.blend_mode      = rec.billboard.blend_mode;
        cfg.base_color_channel = rec.billboard.base_color_channel;
        cfg.front_face      = rec.billboard.front_face_ccw
                              ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                              : VK_FRONT_FACE_CLOCKWISE;
        if (!rec.billboard.texture_id.empty())
            cfg.texture_id = rec.billboard.texture_id;
        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode == TextureSourceMode::Array)
            { cfg.use_texture_array = true; break; }

        std::fprintf(stderr, "[CreateMaterialFromRecord] Billboard preset=%d  use_texture_array=%d  blend=%d\n",
            (int)rec.preset, (int)cfg.use_texture_array, (int)cfg.blend_mode);

        return mm->AcquireMaterial(rec.preset, &cfg);
    }
    // ── 2D ──────────────────────────────────────────────────────────────────
    else if (rec.dim == MaterialAssetRecord::Dim::D2)
    {
        Material2DCreateConfig cfg(
            rec.prim,
            rec.coord_2d,
            rec.l2w ? IncludeL2W::With : IncludeL2W::Without);
        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode != TextureSourceMode::None)
                cfg.SetTextureSourceModeOverride(tc.slot, tc.source_mode);
        return mm->AcquireMaterial(rec.preset, &cfg);
    }
    // ── 3D (含 SkyMinimal / TerrainGrid / Standard / PBR 等) ─────────────────
    else
    {
        Material3DCreateConfig cfg(
            rec.prim,
            rec.camera ? IncludeCamera::With : IncludeCamera::Without,
            rec.l2w    ? IncludeL2W::With    : IncludeL2W::Without,
            rec.sky    ? IncludeSky::With     : IncludeSky::Without);
        cfg.sky_ambient_model = rec.sky_ambient;
        cfg.lighting_model    = rec.lighting;
        if (rec.pos_format.Check())
            cfg.position_format = rec.pos_format;
        for (const auto &tc : rec.textures)
            if (tc.source_mode != TextureSourceMode::None)
                cfg.SetTextureSourceModeOverride(tc.slot, tc.source_mode);
        return mm->AcquireMaterial(rec.preset, &cfg);
    }
} // namespace hgl::graph
}
