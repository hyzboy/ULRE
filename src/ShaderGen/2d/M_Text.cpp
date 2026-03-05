#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/mtl/SamplerName.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uint TextColor;";      //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(uint32);     //材质实例数据大小

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

    vec4 TextColor=unpackUnorm4x8(mi.TextColor);

    float lum=texture(TextureText,Input.TexCoord).r;

    FragColor=vec4( TextColor.rgb*lum,
                    TextColor.a);
})";

    class MaterialText2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;
        ~MaterialText2D()=default;

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
            mci->AddTextureSampler(ShaderStage::Fragment,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::Text);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(mi_codes,                       //材质实例glsl代码
                                     mi_bytes,                       //材质实例数据大小
                                     (uint32_t)ShaderStage::Fragment);  //只在Fragment Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialText
}//namespace

MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,const Text2DMaterialCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.position_format=VAT_IVEC2;
    new_cfg.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    MaterialText2D mt2d(&new_cfg);

    return mt2d.Create(profile);
}
}//namespace hgl::graph::mtl
