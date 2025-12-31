#include <hgl/graph/Sky.h>

namespace hgl::graph
{
    void SkyInfo::SetTime(float hour,float minute,float second)
    {
        std::tm t{};

        t.tm_hour = static_cast<int>(hour);
        t.tm_min  = static_cast<int>(minute);
        t.tm_sec  = static_cast<int>(second);

        const float total_hours = hour + minute / 60.0f + second / 3600.0f;

        // 简单模型：假设太阳在正南方（180度），根据时间计算仰角
        // 正午12点时，太阳在最高点，仰角为90度
        // 早晨6点和傍晚18点，太阳在地平线，仰角为0度
        // 夜晚则太阳在地平线以下，仰角为负值
        float elevation_deg = 0.0f;

        if (total_hours >= 6.0f && total_hours <= 18.0f)
        {
            // 白天：线性插值从0到90再到0
            if (total_hours <= 12.0f)
                elevation_deg = (total_hours - 6.0f) * (90.0f / 6.0f); // 6点到12点
            else
                elevation_deg = (18.0f - total_hours) * (90.0f / 6.0f); // 12点到18点
        }
        else
        {
            // 夜晚：太阳在地平线以下，设为-10度
            elevation_deg = -10.0f;
        }
        // 方位角固定为180度（正南方）
        const float azimuth_deg = 180.0f;
        // 将方位角和仰角转换为方向向量
        const float azimuth_rad   = azimuth_deg * (3.14159265f / 180.0f);
        const float elevation_rad = elevation_deg * (3.14159265f / 180.0f);

        sun_direction.x = cos(elevation_rad) * sin(azimuth_rad);
        sun_direction.y = cos(elevation_rad) * cos(azimuth_rad);
        sun_direction.z = sin(elevation_rad);
        sun_direction.w = 0.0f; // 方向向量

        // 根据仰角调整太阳光强度和颜色（更自然的色温变化）
        if (elevation_deg > 0.0f)
        {
            // 白天：色温随仰角从暖到冷（接近白）
            // elev_norm: 0(地平线) .. 1(正顶)
            const float elev_norm = elevation_deg >= 90.0f ? 1.0f : (elevation_deg <= 0.0f ? 0.0f : elevation_deg/90.0f);
            // 暖色因子：地平线最暖，正顶最冷
            const float warm = powf(1.0f - elev_norm, 1.2f);

            // 日光近白（略暖），黄昏偏橙
            const Color4f day_col (1.00f, 0.98f, 0.95f, 1.0f);
            const Color4f dusk_col(1.00f, 0.62f, 0.32f, 1.0f);

            sun_color.r = day_col.r + (dusk_col.r - day_col.r) * warm;
            sun_color.g = day_col.g + (dusk_col.g - day_col.g) * warm;
            sun_color.b = day_col.b + (dusk_col.b - day_col.b) * warm;
            sun_color.a = 1.0f;

            // 光晕颜色与太阳色一致，稍暖
            halo_color = sun_color;

            // 白天，光强固定为可见（由材质做映射）
            sun_intensity = 1.0f;
            moon_intensity = 0.0f;
        }
        else
        {
            // 夜晚：太阳熄灭，月亮可见（颜色保持不变或使用 moon_color）
            sun_intensity = 0.0f;
            moon_intensity = 1.0f;
        }
    }
} // namespace hgl::graph
