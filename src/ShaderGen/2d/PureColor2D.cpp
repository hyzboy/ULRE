#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/math/Vector.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char mi_codes[]="vec4 Color;";          //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(Vector4f);     //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    HandoverMI();

    gl_Position=GetPosition2D();
})";

    constexpr const char fs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    Color=mi.Color;
})";// ^       ^
    // |       |
    // |       +--ps:这里的mi.Color是材质实例中的数据，MaterialInstance结构对应上面C++代码中的mi_codes
    // +--ps:这里的Color就是最终的RT

    class MaterialPureColor2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;

        ~MaterialPureColor2D()=default;

        bool CreateVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std2DMaterial::CreateVertexShader(vsc))
                return(false);

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CreateFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"Color");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool AfterCreateShader() override
        {
            mci->SetMaterialInstance(   mi_codes,                       //材质实例glsl代码
                                        mi_bytes,                       //材质实例数据大小
                                        VK_SHADER_STAGE_FRAGMENT_BIT);  //只在Fragment Shader中使用材质实例最终数据

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
