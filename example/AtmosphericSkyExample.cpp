// 大气天空材质使用示例 - 改进版本
// Example usage of Atmospheric Sky Material with SkyInfo improvements

#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/mtl/SkyInfo.h>
#include<hgl/graph/VKDevice.h>

// 声明材质创建函数
MaterialCreateInfo *CreateAtmosphericSky3D(const VulkanDevAttr *dev_attr, const Material3DCreateConfig *cfg);

// 使用示例
void CreateAtmosphericSkyExample(VKDevice *device)
{
    // 1. 配置材质创建参数
    Material3DCreateConfig config;
    config.Reset();
    
    // 必需设置：
    config.sky = true;      // 启用天空信息UBO
    config.camera = true;   // 启用相机信息（用于射线计算）
    
    // 可选设置：
    config.position_format = VAF_VEC3;  // 位置格式（全屏四边形使用vec3即可）
    
    // 2. 创建材质
    MaterialCreateInfo *sky_material = CreateAtmosphericSky3D(device->GetDevAttr(), &config);
    
    if(sky_material)
    {
        // 3. 使用材质
        // - 可以应用在全屏四边形上作为天空背景
        // - 或者应用在天空球/天空盒上
        // - 确保在场景的最后渲染（或最早渲染并关闭深度写入）
        
        // 材质会自动：
        // - 使用 exp2(-ray.y/sky.atmosphere_base_colors) 生成大气渐变（基础色来自SkyInfo）
        // - 根据 SkyInfo UBO 中的统一参数渲染太阳/月亮
        //   * sky.sun_direction: 太阳/月亮方向
        //   * sky.sun_color: 太阳/月亮颜色  
        //   * sky.sun_intensity: 光强（0=夜晚显示月亮，>0=白天显示太阳）
        //   * sky.atmosphere_base_colors: 大气散射基础色
        //   * sky.sun_angular_size: 天体视角大小
        //   * sky.celestial_halo_intensity: 光晕强度
        //   * sky.celestial_halo_power: 光晕衰减指数
    }
}

// SkyInfo类使用示例
void SkyInfoUsageExample()
{
    // 创建SkyInfo实例
    SkyInfo sky_info;
    
    // 方法1: 手动设置参数
    sky_info.Reset();  // 重置为默认值
    sky_info.sun_direction = Vector4f(0.3f, 0.8f, 0.5f, 0.0f);
    sky_info.sun_color = Vector4f(1.0f, 0.9f, 0.7f, 1.0f);
    sky_info.atmosphere_base_colors = Vector3f(0.12f, 0.35f, 0.65f);  // 自定义大气色
    
    // 方法2: 根据时间自动设置（推荐）
    float current_time = 0.75f;  // 黄昏时分（0.0=午夜，0.5=正午，1.0=午夜）
    sky_info.SetByTimeOfDay(current_time);
    
    // SetByTimeOfDay会自动：
    // - 计算适合当前时间的atmosphere_base_colors
    // - 设置太阳/月亮的方向、颜色、强度
    // - 调整光晕参数
}

/*
天空信息UBO设置示例（现在支持更多参数）：

// 白天设置
SkyInfo sky_info;
sky_info.SetByTimeOfDay(0.5f);  // 正午
// 结果：
// - atmosphere_base_colors = vec3(0.1, 0.3, 0.6) - 标准蓝天
// - sun_intensity = 1.0 - 全强度太阳
// - sun_color = 暖黄色
// - 适当的光晕参数

// 夜晚设置  
sky_info.SetByTimeOfDay(0.0f);  // 午夜
// 结果：
// - atmosphere_base_colors = vec3(0.05, 0.1, 0.3) - 深蓝夜空
// - sun_intensity = 0.0 - 触发月亮模式
// - sun_color = 冷蓝色调
// - 柔和的光晕参数

// 黄昏设置
sky_info.SetByTimeOfDay(0.75f);  // 黄昏
// 结果：
// - atmosphere_base_colors = vec3(0.3, 0.2, 0.4) - 橙红色调
// - sun_intensity = 较低值 - 暗淡的太阳
// - sun_color = 橙红色太阳
// - 适中的光晕参数
*/