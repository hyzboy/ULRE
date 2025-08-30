#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/Vector3f.h>
#include<hgl/type/Vector4f.h>

STD_MTL_NAMESPACE_BEGIN

/**
 * SkyInfo类：管理天空和天体（太阳/月亮）的渲染信息
 * 
 * 主要功能：
 * 1. 根据时间计算大气散射基础色（替代硬编码的vec3(0.1, 0.3, 0.6)）
 * 2. 统一管理太阳和月亮的参数（基础色、光晕大小等）
 * 3. 提供SetByTimeOfDay方法根据时间自动配置天空参数
 */
class SkyInfo
{
public:
    // 天体（太阳/月亮）方向，指向场景的方向，需归一化，w=0
    Vector4f sun_direction;
    
    // 天体（太阳/月亮）颜色，线性空间 RGBA
    Vector4f sun_color;
    
    // 光强，白天>0，夜晚=0（用于区分太阳/月亮模式）
    float sun_intensity;
    
    // 正午太阳方位角（度），默认 180=南
    float sun_path_azimuth_deg;
    
    // 正午最大仰角（度）
    float max_elevation_deg;
    
    // 大气散射基础色（用于 exp2(-ray.y/atmosphere_base_colors) 计算）
    Vector3f atmosphere_base_colors;
    
    // 太阳/月亮的视角大小
    float sun_angular_size;
    
    // 统一的天体参数（太阳/月亮共用）
    float celestial_halo_intensity;  // 光晕强度
    float celestial_halo_power;      // 光晕衰减指数

public:
    SkyInfo();
    ~SkyInfo() = default;

    /**
     * 根据一天中的时间设置天空参数
     * @param time_of_day 一天中的时间，0.0=午夜，0.5=正午，1.0=午夜
     */
    void SetByTimeOfDay(float time_of_day);
    
    /**
     * 重置为默认值
     */
    void Reset();
    
private:
    /**
     * 根据时间计算大气基础色
     * @param time_of_day 一天中的时间，0.0=午夜，0.5=正午，1.0=午夜
     * @return 大气散射基础色
     */
    Vector3f CalculateAtmosphereColors(float time_of_day) const;
    
    /**
     * 根据时间计算太阳/月亮参数
     * @param time_of_day 一天中的时间，0.0=午夜，0.5=正午，1.0=午夜
     */
    void CalculateCelestialParameters(float time_of_day);
};

STD_MTL_NAMESPACE_END