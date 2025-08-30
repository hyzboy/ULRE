#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // 大气渲染材质 - 超简易版本
    // 实现简单的大气渐变背景和太阳/月亮渲染
    
    constexpr const char vs_main[]=R"(
void main()
{
    // 直接使用屏幕空间四边形，不需要变换
    gl_Position = vec4(GetPosition3D().xy, 0.999, 1.0);  // 放在远平面附近
    
    // 计算射线方向，从相机位置指向屏幕空间的点
    float aspect = viewport.canvas_resolution.x / viewport.canvas_resolution.y;
    Output.RayDirection = normalize(vec3(GetPosition3D().xy * vec2(aspect, 1.0), -1.0));
})";

    constexpr const char fs_main[]=R"(
void main()
{
    vec3 ray = normalize(Input.RayDirection);
    
    // 简单的大气渐变背景：return exp2(-ray.y/vec3(.1,.3,.6));
    vec3 atmosphere_color = exp2(-ray.y / vec3(0.1, 0.3, 0.6));
    
    // 如果启用了天空信息，渲染太阳/月亮
    vec3 final_color = atmosphere_color;
    
    // 获取太阳方向（从太阳指向场景的方向，所以需要取反得到从场景指向太阳的方向）
    vec3 sun_dir = -normalize(sky.sun_direction.xyz);
    
    // 计算射线与太阳方向的夹角
    float sun_dot = dot(ray, sun_dir);
    
    // 太阳光盘 - 使用简单的圆形范围
    float sun_angular_size = 0.03; // 太阳的视角大小
    float sun_mask = smoothstep(cos(sun_angular_size + 0.01), cos(sun_angular_size), sun_dot);
    
    // 太阳光晕效果
    float sun_halo = pow(max(sun_dot, 0.0), 16.0) * 0.3;
    
    // 混合太阳颜色
    if(sky.sun_intensity > 0.0)
    {
        vec3 sun_contribution = sky.sun_color.rgb * sky.sun_intensity * (sun_mask + sun_halo);
        final_color += sun_contribution;
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