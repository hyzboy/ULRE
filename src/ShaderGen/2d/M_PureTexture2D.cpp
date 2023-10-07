#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/SamplerName.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.TexCoord=TexCoord;

    gl_Position=GetPosition2D();
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(
void main()
{
    Color=texture(TextureColor,Input.TexCoord);
})";

    class MaterialPureTexture2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;
        ~MaterialPureTexture2D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std2DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddInput(VAT_VEC2,VAN::TexCoord);

            vsc->AddOutput(VAT_VEC2,"TexCoord");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::Color);

            fsc->AddOutput(VAT_VEC4,"Color");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialPureTexture2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreatePureTexture2D(const mtl::Material2DCreateConfig *cfg)
{
    MaterialPureTexture2D mvc2d(cfg);

    return mvc2d.Create();
}
STD_MTL_NAMESPACE_END
