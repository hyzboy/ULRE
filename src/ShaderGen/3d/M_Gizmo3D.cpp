#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    // Gizmo3D材质其实就是纯色的blinnphong材质，但不需要外部传入太阳光方向、高光系数等数据。
    // 其全部在Shader中直接包含，它是专门为Gizmo 3D控件所准备的一种材质。
    
    constexpr const char mi_codes[]="vec4 Color;";                      //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(Vector4f);                 //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    HandoverMI();

    Output.Normal   =GetNormal();
    Output.Position =GetPosition3D();

    gl_Position     =Output.Position;
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(

const vec3 SUN_DIRECTION=vec3(0.655386,0.491539,0.573462);      //normalized(8,6,7)
const vec3 SUN_COLOR=vec3(1.0,1.0,1.0);

void main()
{
    MaterialInstance mi=GetMI();

    //点乘法线和光照
    float intensity=0.5*max(dot(Input.Normal,SUN_DIRECTION),0.0)+0.5;

    //直接光颜色
    vec3 direct_color=intensity*SUN_COLOR*mi.Color.rgb;

    vec3 spec_color=vec3(0);

    if(intensity>0.0)
    {
        vec3 half_vector=normalize(SUN_DIRECTION+normalize(Input.Position.xyz+camera.pos));

        float specular=max(dot(half_vector,Input.Normal),0.0);

        spec_color=specular*pow(specular,16)*SUN_COLOR;
    }
        
    FragColor=vec4(direct_color+spec_color,1.0);
})";

    class MaterialGizmo3D:public Std3DMaterial
    {
    public:

        using Std3DMaterial::Std3DMaterial;
        ~MaterialGizmo3D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            vsc->AddInput(VAT_VEC3,VAN::Normal);

            vsc->AddOutput(SVT_VEC4,"Position");
            vsc->AddOutput(SVT_VEC3,"Normal");

            if(!Std3DMaterial::CustomVertexShader(vsc))     //会根据是否有GetNormal函数来决定是否添加Normal计算代码，所以需要放在AddInput后面
                return(false);

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(mi_codes,                       //材质实例glsl代码
                                     mi_bytes,                       //材质实例数据大小
                                     (uint32_t)ShaderStage::Fragment);  //只在Vertex Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialGizmo3D:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateGizmo3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *cfg)
{
    MaterialGizmo3D mg3d(cfg);

    return mg3d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
