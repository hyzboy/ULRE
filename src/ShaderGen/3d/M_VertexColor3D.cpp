#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.Color=Color;

    gl_Position=GetPosition3D();
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=Input.Color;
})";// ^       ^
    // |       |
    // |       +--ps:这里的Input.Color就是上一个Shader中的Output.Color
    // +--ps:这里的Color就是最终的RT

    class MaterialVertexColor3D:public Std3DMaterial
    {
    public:

        using Std3DMaterial::Std3DMaterial;
        ~MaterialVertexColor3D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddInput(VAT_VEC4,VAN::Color);

            vsc->AddOutput(SVT_VEC4,"Color");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialVertexColor3D:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateVertexColor3D(const GPUDeviceAttribute *dev_attr,const Material3DCreateConfig *cfg)
{
    MaterialVertexColor3D mvc3d(cfg);

    return mvc3d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
