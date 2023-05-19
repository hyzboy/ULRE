#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    HandoverMI();               //交接材质实例ID给下一个Shader

    gl_Position=GetPosition2D();
})";

    constexpr const char fs_main[]=R"(


void main()
{
    MaterialInstance mi=GetMI();

    Color=mi.Color;
})";

    class MaterialPureColor2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;
        ~MaterialPureColor2D()=default;

        bool CreateVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std2DMaterial::CreateVertexShader(vsc))
                return(false);

            vsc->AddFunction(vs_main);
            return(true);
        }

        bool CreateFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"Color");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->AddFunction(fs_main);
            return(true);
        }
    };//class MaterialPureColor2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreatePureColor2D(const Material2DConfig *cfg)
{
    MaterialPureColor2D mpc2d(cfg);

    return mpc2d.Create();
}
STD_MTL_NAMESPACE_END
