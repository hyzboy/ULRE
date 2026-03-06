#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include"common/MFRectPrimitive.h"
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uvec4 id;";          //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4u);       //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    HandoverMI();

    Output.TexCoord=TexCoord;
    gl_Position=GetPosition2D();
})";

    constexpr const char fs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    FragColor=texture(TextureBaseColor,vec3(Input.TexCoord,mi.id.x));
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
            mci->AddTextureSampler(ShaderStage::Fragment,DescriptorSetType::PerMaterial,SamplerType::Sampler2DArray,mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(   mi_codes,                       //材质实例glsl代码
                                        mi_bytes,                       //材质实例数据大小
                                        (uint32_t)ShaderStage::Fragment);  //只在Fragment Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialRectTexture2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,mtl::Material2DCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->prim=PrimitiveType::Triangles;
    cfg->material_instance=true;
    cfg->position_format=VAT_VEC2;
    cfg->shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    MaterialRectTexture2D mvc2d(cfg);

    return mvc2d.Create(profile);
}
}//namespace hgl::graph::mtl
