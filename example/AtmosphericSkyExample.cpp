// 大气天空材质使用示例
// Example usage of Atmospheric Sky Material

#include<hgl/graph/mtl/Material3DCreateConfig.h>
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
        // - 使用 exp2(-ray.y/vec3(.1,.3,.6)) 生成大气渐变
        // - 根据 SkyInfo UBO 中的太阳参数渲染太阳/月亮
        //   * sky.sun_direction: 太阳方向
        //   * sky.sun_color: 太阳颜色  
        //   * sky.sun_intensity: 光强（0=夜晚显示月亮，>0=白天显示太阳）
    }
}

/*
天空信息UBO设置示例：

// 白天设置
SkyInfo sky_info;
sky_info.sun_direction = vec4(normalize(vec3(0.3, 0.8, 0.5)), 0);  // 太阳方向（归一化）
sky_info.sun_color = vec4(1.0, 0.95, 0.8, 1.0);                   // 暖黄色太阳
sky_info.sun_intensity = 1.0;                                      // 全强度

// 夜晚设置  
sky_info.sun_direction = vec4(normalize(vec3(-0.3, 0.6, -0.5)), 0); // 月亮方向
sky_info.sun_color = vec4(0.8, 0.8, 1.0, 1.0);                     // 冷蓝色调
sky_info.sun_intensity = 0.0;                                       // 强度为0触发月亮模式

// 黄昏设置
sky_info.sun_direction = vec4(normalize(vec3(0.8, 0.1, 0.6)), 0);   // 低角度太阳
sky_info.sun_color = vec4(1.0, 0.6, 0.3, 1.0);                     // 橙红色太阳  
sky_info.sun_intensity = 0.3;                                       // 较低强度
*/