#include<hgl/graph/mtl/SkyInfo.h>
#include<cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

STD_MTL_NAMESPACE_BEGIN

SkyInfo::SkyInfo()
{
    Reset();
}

void SkyInfo::Reset()
{
    // 默认太阳参数
    sun_direction = Vector4f(0.3f, 0.8f, 0.5f, 0.0f);  // 默认太阳方向（归一化）
    sun_color = Vector4f(1.0f, 0.95f, 0.8f, 1.0f);     // 暖黄色太阳
    sun_intensity = 1.0f;                               // 全强度（白天）
    
    sun_path_azimuth_deg = 180.0f;                      // 正午方位角，南方
    max_elevation_deg = 60.0f;                          // 正午最大仰角
    
    // 默认大气散射基础色（替代原硬编码的 vec3(0.1, 0.3, 0.6)）
    atmosphere_base_colors = Vector3f(0.1f, 0.3f, 0.6f);
    
    // 太阳/月亮视角大小和光晕参数
    sun_angular_size = 0.025f;                          // 约1.4度
    celestial_halo_intensity = 0.2f;                    // 光晕强度
    celestial_halo_power = 32.0f;                       // 光晕衰减指数
}

void SkyInfo::SetByTimeOfDay(float time_of_day)
{
    // 确保time_of_day在[0,1]范围内
    time_of_day = fmod(time_of_day, 1.0f);
    if (time_of_day < 0.0f) time_of_day += 1.0f;
    
    // 根据时间计算大气基础色
    atmosphere_base_colors = CalculateAtmosphereColors(time_of_day);
    
    // 根据时间计算太阳/月亮参数
    CalculateCelestialParameters(time_of_day);
}

Vector3f SkyInfo::CalculateAtmosphereColors(float time_of_day) const
{
    // 一天中不同时间的大气基础色配置
    // 0.0 = 午夜, 0.25 = 黎明, 0.5 = 正午, 0.75 = 黄昏, 1.0 = 午夜
    
    Vector3f night_colors(0.05f, 0.1f, 0.3f);       // 夜晚：深蓝色调
    Vector3f dawn_colors(0.15f, 0.25f, 0.5f);       // 黎明：蓝紫色调  
    Vector3f day_colors(0.1f, 0.3f, 0.6f);          // 白天：标准蓝天（原硬编码值）
    Vector3f dusk_colors(0.3f, 0.2f, 0.4f);         // 黄昏：橙红色调
    
    Vector3f result;
    
    // 手动实现lerp: a + t * (b - a)
    auto lerp = [](const Vector3f& a, const Vector3f& b, float t) -> Vector3f {
        return Vector3f(
            a.x + t * (b.x - a.x),
            a.y + t * (b.y - a.y),
            a.z + t * (b.z - a.z)
        );
    };
    
    if (time_of_day < 0.25f) {
        // 午夜到黎明 (0.0 - 0.25)
        float t = time_of_day * 4.0f;  // 映射到[0,1]
        result = lerp(night_colors, dawn_colors, t);
    } else if (time_of_day < 0.5f) {
        // 黎明到正午 (0.25 - 0.5)
        float t = (time_of_day - 0.25f) * 4.0f;  // 映射到[0,1]
        result = lerp(dawn_colors, day_colors, t);
    } else if (time_of_day < 0.75f) {
        // 正午到黄昏 (0.5 - 0.75)
        float t = (time_of_day - 0.5f) * 4.0f;  // 映射到[0,1]
        result = lerp(day_colors, dusk_colors, t);
    } else {
        // 黄昏到午夜 (0.75 - 1.0)
        float t = (time_of_day - 0.75f) * 4.0f;  // 映射到[0,1]
        result = lerp(dusk_colors, night_colors, t);
    }
    
    return result;
}

void SkyInfo::CalculateCelestialParameters(float time_of_day)
{
    // 计算太阳角度（简化的太阳轨迹）
    // 0.0 = 午夜（太阳在地平线下），0.5 = 正午（太阳最高点）
    
    // 使用余弦函数：cos(2π*time) 在time=0和time=1时为1，在time=0.5时为-1
    // 我们要反过来：cos(2π*(time+0.5)) 在time=0时为-1，在time=0.5时为1  
    float day_progress = (time_of_day + 0.5f) * 2.0f * M_PI;  
    float elevation_factor = cos(day_progress);  // -1 (midnight) to +1 (noon)
    float elevation = elevation_factor * max_elevation_deg * M_PI / 180.0f;  // 仰角（弧度）
    float azimuth = sun_path_azimuth_deg * M_PI / 180.0f;  // 方位角（弧度）
    
    // 计算太阳方向向量（从太阳指向场景）
    float cos_elev = cos(elevation);
    sun_direction.x = cos_elev * sin(azimuth);
    sun_direction.y = sin(elevation);
    sun_direction.z = cos_elev * cos(azimuth);
    sun_direction.w = 0.0f;
    
    // 归一化方向向量
    float length = sqrt(sun_direction.x * sun_direction.x + 
                       sun_direction.y * sun_direction.y + 
                       sun_direction.z * sun_direction.z);
    if (length > 0.0f) {
        sun_direction.x /= length;
        sun_direction.y /= length;
        sun_direction.z /= length;
    }
    
    // 根据太阳高度计算强度和颜色
    if (elevation > 0.0f) {
        // 白天：太阳在地平线以上
        sun_intensity = sin(elevation);  // 正午最强，日出日落最弱
        
        // 根据太阳高度调整颜色温度
        float color_temp = sin(elevation);
        sun_color.x = 1.0f;                                    // 红分量保持满值
        sun_color.y = 0.85f + 0.15f * color_temp;              // 绿分量随高度增加
        sun_color.z = 0.6f + 0.4f * color_temp;                // 蓝分量随高度增加
        sun_color.w = 1.0f;
        
        // 调整光晕参数
        celestial_halo_intensity = 0.2f * sun_intensity;
        celestial_halo_power = 32.0f;
    } else {
        // 夜晚：太阳在地平线以下，显示月亮
        sun_intensity = 0.0f;
        
        // 计算月亮位置（简化：月亮在太阳对面）
        sun_direction.x = -sun_direction.x;
        sun_direction.y = (sun_direction.y < 0.0f) ? -sun_direction.y : sun_direction.y;  // 月亮总是在地平线以上
        sun_direction.z = -sun_direction.z;
        
        // 月亮颜色：冷蓝色调
        sun_color = Vector4f(0.8f, 0.8f, 1.0f, 1.0f);
        
        // 月亮光晕参数
        celestial_halo_intensity = 0.1f;
        celestial_halo_power = 16.0f;  // 月亮光晕更柔和
    }
}

STD_MTL_NAMESPACE_END