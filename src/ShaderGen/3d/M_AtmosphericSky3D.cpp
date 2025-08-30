#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // 大气渲染材质生产器 - 改进版本
    // 
    // 功能：
    // 1. 背景使用 exp2(-ray.y/sky.atmosphere_base_colors) 模拟大气渐变色，基础色由SkyInfo传入
    // 2. 根据 SkyInfo 的 UBO (在shader中叫sky) 画出太阳或月亮，统一使用相同的参数
    //    - 天体方向：sky.sun_direction
    //    - 天体颜色：sky.sun_color 
    //    - 光强：sky.sun_intensity (夜晚为0时显示月亮，>0时显示太阳)
    //    - 视角大小：sky.sun_angular_size
    //    - 光晕参数：sky.celestial_halo_intensity, sky.celestial_halo_power
    //
    // 使用方法：
    // 1. 确保 Material3DCreateConfig 中 sky = true
    // 2. 调用 CreateAtmosphericSky3D() 创建材质
    // 3. 在全屏四边形或天空球上使用此材质
    
    constexpr const char vs_main[]=R"(
void main()
{
    // 使用屏幕空间四边形渲染天空
    gl_Position = vec4(GetPosition3D().xy, 0.999, 1.0);  // 放在远平面附近
    
    // 计算射线方向：从NDC空间转换到世界空间方向
    vec2 ndc = GetPosition3D().xy;
    
    // 使用相机的逆投影矩阵计算射线方向
    vec4 clip_space = vec4(ndc, -1.0, 1.0);
    vec4 view_space = camera.inverse_projection * clip_space;
    view_space = vec4(view_space.xy / view_space.w, -1.0, 0.0);
    
    // 转换到世界空间
    Output.RayDirection = (camera.inverse_view * view_space).xyz;
})";

    constexpr const char fs_main[]=R"(
void main()
{
    vec3 ray = normalize(Input.RayDirection);
    
    // 大气渐变背景：使用SkyInfo传入的基础色替代硬编码值
    vec3 atmosphere_color = exp2(-ray.y / sky.atmosphere_base_colors);
    
    // 根据SkyInfo的UBO（在shader中叫sky）画出太阳或月亮
    vec3 final_color = atmosphere_color;
    
    // 获取太阳/月亮方向（UBO中存储的是从天体指向场景的方向，所以需要取反）
    vec3 celestial_dir = -normalize(sky.sun_direction.xyz);
    
    // 计算射线与天体方向的点积
    float celestial_dot = dot(ray, celestial_dir);
    
    // 天体光盘渲染 - 使用UBO中的视角大小参数
    float celestial_mask = smoothstep(cos(sky.sun_angular_size + 0.005), cos(sky.sun_angular_size), celestial_dot);
    
    // 天体光晕效果 - 使用UBO中的光晕参数
    float celestial_halo = pow(max(celestial_dot, 0.0), sky.celestial_halo_power) * sky.celestial_halo_intensity;
    
    // 统一的天体渲染逻辑（太阳/月亮共用）
    if(sky.sun_intensity > 0.0)
    {
        // 太阳模式：强度大于0时渲染太阳
        vec3 celestial_contribution = sky.sun_color.rgb * sky.sun_intensity * (celestial_mask * 2.0 + celestial_halo);
        final_color += celestial_contribution;
    }
    else
    {
        // 月亮模式：强度为0时渲染月亮，使用相同的参数但不同的强度
        vec3 celestial_contribution = sky.sun_color.rgb * 0.3 * (celestial_mask + celestial_halo);
        final_color += celestial_contribution;
    }
    
    FragColor = vec4(final_color, 1.0);
})";

    class MaterialAtmosphericSky3D : public Std3DMaterial
    {
    public:
        using Std3DMaterial::Std3DMaterial;
        ~MaterialAtmosphericSky3D() = default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return false;

            // 输出射线方向到片段着色器
            vsc->AddOutput(SVT_VEC3, "RayDirection");
            
            vsc->SetMain(vs_main);
            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4, "FragColor");
            fsc->SetMain(fs_main);
            return true;
        }

        bool EndCustomShader() override
        {
            // 确保天空UBO可用
            // 这会在Std3DMaterial::CustomVertexShader中根据cfg->sky自动添加
            return true;
        }
    }; // class MaterialAtmosphericSky3D
} // namespace

MaterialCreateInfo *CreateAtmosphericSky3D(const VulkanDevAttr *dev_attr, const Material3DCreateConfig *cfg)
{
    // 创建配置副本并确保启用天空信息
    Material3DCreateConfig config = *cfg;
    config.sky = true;
    config.camera = true;  // 需要相机信息用于射线计算
    
    MaterialAtmosphericSky3D mas3d(&config);
    return mas3d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END