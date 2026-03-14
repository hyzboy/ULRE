#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.TexCoord=TexCoord;
    gl_Position=GetPosition2D();
})";

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=texture(TextureBaseColor,Input.TexCoord);
})";

    class MaterialRectTexture2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;
        ~MaterialRectTexture2D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std2DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddInput(VAT_VEC2,VAN::TexCoord);

            vsc->AddOutput(SVT_VEC2,"TexCoord");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            mci->AddTextureSampler(ShaderStage::Fragment,DescriptorSetType::Material,SamplerType::Sampler2D,mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialRectTexture2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreateRectTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.position_format=VAT_VEC2;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    MaterialRectTexture2D mvc2d(&inner);

    return mvc2d.Create(profile);
}
}//namespace hgl::graph::mtl
