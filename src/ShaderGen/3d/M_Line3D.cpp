#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // 顶点着色器 - 接收点数据并传递给几何着色器
    // Position.xyz = 起点, Normal.xyz = 终点
    constexpr const char vs_main[]=R"(
void main()
{
    Output.StartPosition = StartPosition;  // 起点位置
    Output.EndPosition = EndPosition;      // 终点位置（重用Normal属性存储终点）
    Output.Color = Color;        // 颜色
    
    // 传递起点作为几何着色器的输入点
    gl_Position = vec4(Position, 1);
})";

    // 几何着色器 - 将点转换为线段
    constexpr const char gs_main[]=R"(
void main()
{
    // 生成线段的起点
    gl_Position = camera.vp * vec4(Input[0].StartPosition, 1);
    Output.Color = Input[0].Color;
    EmitVertex();
    
    // 生成线段的终点
    gl_Position = camera.vp * vec4(Input[0].EndPosition, 1);
    Output.Color = Input[0].Color;
    EmitVertex();
    
    EndPrimitive();
})";

    // 片段着色器 - 输出颜色
    constexpr const char fs_main[]=R"(
void main()
{
    FragColor = Input.Color;
})";

    class MaterialLine3D:public Std3DMaterial
    {
        Line3DMaterialCreateConfig *line_config;

    public:

        MaterialLine3D(Line3DMaterialCreateConfig *lcfg):Std3DMaterial(lcfg)
        {
            line_config = lcfg;
        }
        ~MaterialLine3D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            // 输入：起点、终点、颜色
            vsc->AddInput(VAT_VEC3, VAN::StartPosition);    // 线段起点
            vsc->AddInput(VAT_VEC3, VAN::EndPosition);      // 线段终点
            vsc->AddInput(VAT_VEC4, VAN::Color);            // 线段颜色

            // 输出到几何着色器
            vsc->AddOutput(SVT_VEC3, VAN::StartPosition);
            vsc->AddOutput(SVT_VEC3, VAN::EndPosition);
            vsc->AddOutput(SVT_VEC4, VAN::Color);

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            // 输入：POINTS，输出：LINES，每个点生成2个顶点（线段的两端）
            gsc->SetGeom(PrimitiveType::Points, PrimitiveType::Lines, 2);

            // 输出到片段着色器
            gsc->AddOutput(SVT_VEC4, VAN::Color);

            gsc->SetMain(gs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4, "FragColor");

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialLine3D:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateLine3D(const VulkanDevAttr *dev_attr, Line3DMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    MaterialLine3D line3d(cfg);

    return line3d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
