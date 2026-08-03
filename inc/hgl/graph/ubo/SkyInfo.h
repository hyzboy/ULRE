#pragma once

#include <ctime>
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

        void SetTime(float hour, float minute, float second);
    };
}
