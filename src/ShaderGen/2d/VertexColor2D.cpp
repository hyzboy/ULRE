#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.Color=Color;

    gl_Position=GetPosition2D();
})";

    //一个shader中所有向下一个shader输出，会被定义在一个名为Output的结构中。
    //而同时，这个Output结构，会在下一个Shader中，以Input名称出现使用。
    //也就是说：在此材质中，VertexShader中的Output等于FragmentShader中的Input

    constexpr const char fs_main[]=R"(
void main()
{
    Color=Input.Color;
})";

    class MaterialVertexColor2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;
        ~MaterialVertexColor2D()=default;

        bool CreateVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std2DMaterial::CreateVertexShader(vsc))
                return(false);

            vsc->AddInput(VAT_VEC4,VAN::Color);

            vsc->AddOutput(VAT_VEC4,"Color");

            vsc->AddFunction(vs_main);
            return(true);
        }

        bool CreateFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"Color");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->AddFunction(fs_main);
            return(true);
        }
    };//class MaterialVertexColor2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreateVertexColor2D(const Material2DConfig *cfg)
{
    MaterialVertexColor2D mvc2d(cfg);

    return mvc2d.Create();
}
STD_MTL_NAMESPACE_END
