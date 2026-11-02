#include<hgl/ecs/systems/render/light_shadow/SkyEnvironmentConfig.h>

namespace hgl::ecs
{
    const char *GetSkyModeName(SkyMode mode) noexcept
    {
        switch (mode)
        {
            case SkyMode::Atmosphere:      return "Atmosphere";
            case SkyMode::CubeMapIBL:      return "CubeMapIBL";
            case SkyMode::OctahedralIBL:   return "OctahedralIBL";
            case SkyMode::LambertDome:     return "LambertDome";
            case SkyMode::SHLightOnly:     return "SHLightOnly";
            case SkyMode::None:            return "None";
        }

        return "Unknown";
    }

    const char *GetSkyModeGeometryURI(SkyMode mode) noexcept
    {
        switch (mode)
        {
            // 全天球（只喂 POSITION，UV 由方向在几何/着色器侧推导）
            case SkyMode::Atmosphere:
            case SkyMode::CubeMapIBL:
            case SkyMode::OctahedralIBL:   return "geo:builtin:hex_sphere_sky";

            // 半球穹顶（顶点自带方位投影 UV，材质侧就是普通 2D 采样）
            case SkyMode::LambertDome:     return "geo:builtin:dome_64";

            case SkyMode::SHLightOnly:     return nullptr;
            case SkyMode::None:            return nullptr;
        }

        return nullptr;
    }

    bool SkyModeGeometryHasUV(SkyMode mode) noexcept
    {
        // 只有半球穹顶的 UV 是**几何事实**（按 Lambert Azimuthal 投影烘进顶点）；
        // 全天球三兄弟靠方向采样，声明 UV 反而要求几何提供它没有的流。
        return mode == SkyMode::LambertDome;
    }

    bool SkyEnvironmentConfig::IsSHZero() const
    {
        for (int i = 0; i < 9; ++i)
        {
            if (sh[i].x != 0.0f || sh[i].y != 0.0f || sh[i].z != 0.0f)
                return false;
        }

        return true;
    }

    void ApplyEnvironmentSpec(const graph::EnvironmentSpec &spec, SkyEnvironmentConfig &cfg)
    {
        // ── [visual] ────────────────────────────────────────────────────────────
        if (spec.visual.recipe.present)
            cfg.sky_recipe = spec.visual.recipe.value;

        if (spec.visual.geometry.present)
            cfg.sky_geometry_uri = spec.visual.geometry.value;

        if (spec.visual.scale.present)
            cfg.sky_scale = spec.visual.scale.value;

        if (spec.visual.track_camera.present)
            cfg.track_camera = spec.visual.track_camera.value;

        // ── [ambient] ───────────────────────────────────────────────────────────
        if (spec.ambient.mode.present)
            cfg.mode = spec.ambient.mode.value;

        if (spec.ambient.provider.present)
            cfg.ambient_provider = spec.ambient.provider.value;

        // ── [ibl] ───────────────────────────────────────────────────────────────
        if (spec.ibl.irradiance.present)
            cfg.irradiance_texture = spec.ibl.irradiance.value;

        if (spec.ibl.prefilter.present)
            cfg.prefilter_texture = spec.ibl.prefilter.value;

        if (spec.ibl.brdf_lut.present)
            cfg.brdf_lut_texture = spec.ibl.brdf_lut.value;

        // ── [ldr] ───────────────────────────────────────────────────────────────
        if (spec.ldr.mode.present)
            cfg.ldr = spec.ldr.mode.value;

        if (spec.ldr.exposure_base.present)
            cfg.ldr_exposure_base = spec.ldr.exposure_base.value;

        if (spec.ldr.sun_scale.present)
            cfg.ldr_sun_scale = spec.ldr.sun_scale.value;

        if (spec.ldr.min_ambient.present)
            cfg.ldr_min_ambient = spec.ldr.min_ambient.value;

        // ── [sh9] ───────────────────────────────────────────────────────────────
        if (spec.sh9.auto_generate.present)
            cfg.sh_auto = spec.sh9.auto_generate.value;

        if (spec.sh9.track_sun.present)
            cfg.sh_track_sun = spec.sh9.track_sun.value;

        if (spec.sh9.sun_irradiance_scale.present)
            cfg.sh_sun_irradiance_scale = spec.sh9.sun_irradiance_scale.value;

        if (spec.sh9.exposure.present)
            cfg.sh_params.exposure = spec.sh9.exposure.value;

        // 空数组与缺键等价（与注册表校验同口径）：`bands = []` 不应把干净的天光清成 0。
        if (spec.sh9.bands.present && spec.sh9.bands.value.GetCount() > 0)
        {
            // 整组覆盖：缺项补 0 而不是保留旧值 —— 半新半旧的系数没有任何物理含义。
            for (int i = 0; i < 9; ++i)
            {
                const graph::EnvironmentSH9BandSpec *band =
                    (i < static_cast<int>(spec.sh9.bands.value.GetCount()))
                        ? spec.sh9.bands.value.GetData() + i
                        : nullptr;

                cfg.sh[i] = band
                    ? math::Vector4f(band->r, band->g, band->b, 0.0f)
                    : math::Vector4f(0.0f);
            }
        }
    }
}//namespace hgl::ecs
