#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // 大气渲染材质生产器 - 超简易版本
    // 
    // 功能：
    // 1. 背景使用 exp2(-ray.y/vec3(.1,.3,.6)) 模拟大气渐变色
    // 2. 根据 SkyInfo 的 UBO (在shader中叫sky) 画出太阳或月亮
    //    - 太阳方向：sky.sun_direction
    //    - 太阳颜色：sky.sun_color 
    //    - 光强：sky.sun_intensity (夜晚为0时可显示月亮)
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
    
    // 大气渐变背景：使用指定的公式 exp2(-ray.y/vec3(.1,.3,.6)) 模拟渐变色
    vec3 atmosphere_color = exp2(-ray.y / vec3(0.1, 0.3, 0.6));
    
    // 根据SkyInfo的UBO（在shader中叫sky）画出太阳或月亮
    vec3 final_color = atmosphere_color;
    
    // 获取太阳方向（UBO中存储的是从太阳指向场景的方向，所以需要取反）
    vec3 sun_dir = -normalize(sky.sun_direction.xyz);
    
    // 计算射线与太阳方向的点积
    float sun_dot = dot(ray, sun_dir);
    
    // 太阳光盘渲染 - 简单的圆形
    float sun_angular_size = 0.025; // 太阳的视角大小（约1.4度）
    float sun_mask = smoothstep(cos(sun_angular_size + 0.005), cos(sun_angular_size), sun_dot);
    
    // 太阳光晕效果
    float sun_halo = pow(max(sun_dot, 0.0), 32.0) * 0.2;
    
    // 根据太阳强度决定是否渲染太阳/月亮
    if(sky.sun_intensity > 0.0)
    {
        // 太阳模式：强度大于0时渲染明亮的太阳
        vec3 sun_contribution = sky.sun_color.rgb * sky.sun_intensity * (sun_mask * 2.0 + sun_halo);
        final_color += sun_contribution;
    }
    else
    {
        // 月亮模式：强度为0时可以渲染微弱的月亮（可选）
        vec3 moon_contribution = vec3(0.8, 0.8, 1.0) * 0.3 * sun_mask;
        final_color += moon_contribution;
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