#pragma once
#include <ctime>                   // std::tm
#include <hgl/color/Color.h>
#include <hgl/math/Vector.h>
#include <hgl/util/geo/GeoLocation.h>

namespace hgl::graph
{
    using namespace math;

    struct SkyInfo
    {
        Color4f     base_sky_color  =Color4f(0.1f, 0.3f, 0.6f, 1.0f); // 天空基础色（由天气/时间驱动）
        Vector4f    sun_direction   =Vector4f(0, 0, 1, 0);     // w=0 表示方向向量
        Color4f     sun_color       =Color4f(1, 0.95f, 0.9f, 1);
        Color4f     halo_color      =Color4f(1.0f, 0.9f, 0.7f, 1.0f);

        Color4f     moon_color      =Color4f(0.6f,0.7f,0.8f,1);

        float       sun_ang_deg     =16.0f;               // 太阳视直径（度），约0.5度，稍微放大一些
        float       sun_intensity   =1.0f;                // 光强，夜晚为0
        float       moon_intensity  =0.0f;                // 月亮强度
        float       halo_intensity  =0.1f;                // 光晕强度

    public:

        void SetTime(float hour,float minute,float second);
    };
} // namespace hgl::graph
