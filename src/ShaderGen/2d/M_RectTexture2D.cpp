#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/mtl/SamplerName.h>

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
            mci->AddTextureSampler(ShaderStage::Fragment,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialRectTexture2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreateRectTexture2D(const VulkanDevAttr *dev_attr,mtl::Material2DCreateConfig *cfg)
{
    if(!dev_attr||!cfg)
        return(nullptr);

    cfg->prim=PrimitiveType::Triangles;
    cfg->position_format=VAT_VEC2;
    cfg->shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    MaterialRectTexture2D mvc2d(cfg);

    return mvc2d.Create(dev_attr);
}
}//namespace hgl::graph::mtl
