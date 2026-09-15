#pragma once

#include <hgl/color/Color.h>
#include <hgl/math/Vector.h>
#include <hgl/util/geo/GeoLocation.h>

namespace hgl::graph
{
    using namespace math;

    struct SkyInfo
    {
        Color4f     base_sky_color = Color4f(0.1f, 0.3f, 0.6f, 1.0f);
        Vector4f    sun_direction  = Vector4f(0, 0, 1, 0);
        Color4f     sun_color      = Color4f(1, 0.95f, 0.9f, 1);
        Color4f     halo_color     = Color4f(1.0f, 0.9f, 0.7f, 1.0f);
        Color4f     moon_color     = Color4f(0.6f, 0.7f, 0.8f, 1);

        float       sun_ang_deg    = 16.0f;
        float       sun_intensity  = 1.0f;
        float       moon_intensity = 0.0f;
        float       halo_intensity = 0.1f;

        /**
         * IBL 环境贴图 bindless 句柄(1-based,0=未绑定):
         *   x = Diffuse Irradiance Cubemap
         *   y = Specular Prefiltered Cubemap
         *   z = BRDF LUT(2D,(NdotV,roughness) → (scale,bias))
         *   w = 预留
         * 由 IBL ambient 光照模块(light/indirect_ibl.glsl)经 sky.env_tex 采样。
         */
        Vector4u    env_tex        = Vector4u(0);

        void SetTime(float hour, float minute, float second);
    };
}
