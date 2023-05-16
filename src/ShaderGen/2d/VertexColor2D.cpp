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

            vsc->AddOutput(VAT_VEC4,"Color");
            vsc->AddInput(VAT_VEC4,VAN::Color);

            vsc->AddFunction(vs_main);
            return(true);
        }

        bool CreateFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"Color");

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
