#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/mtl/SamplerName.h>

STD_MTL_NAMESPACE_BEGIN
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

    constexpr const char gs_main[]=R"(
void main()
{
    vec2 vlt=gl_in[0].gl_Position.xy;
    vec2 vrb=gl_in[0].gl_Position.zw;
    vec2 tlt=Input[0].TexCoord.xy;
    vec2 trb=Input[0].TexCoord.zw;

    HandoverMI();gl_Position=vec4(vlt,           vec2(0,1));Output.TexCoord=tlt;                EmitVertex();
    HandoverMI();gl_Position=vec4(vlt.x, vrb.y,  vec2(0,1));Output.TexCoord=vec2(tlt.x,trb.y);  EmitVertex();
    HandoverMI();gl_Position=vec4(vrb.x, vlt.y,  vec2(0,1));Output.TexCoord=vec2(trb.x,tlt.y);  EmitVertex();
    HandoverMI();gl_Position=vec4(vrb,           vec2(0,1));Output.TexCoord=trb;                EmitVertex();

    EndPrimitive();
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

            vsc->AddInput(VAT_VEC4,VAN::TexCoord);
            vsc->AddOutput(SVT_VEC4,"TexCoord");
            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            gsc->SetGeom(PrimitiveType::Points,PrimitiveType::TriangleStrip,4);
            gsc->AddOutput(SVT_VEC2,"TexCoord");
            gsc->SetMain(gs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            mci->AddSampler(ShaderStage::Fragment,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::Text);

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

MaterialCreateInfo *CreateText2D(const VulkanDevAttr *dev_attr,const Text2DMaterialCreateConfig *cfg)
{
    if(!dev_attr||!cfg)
        return(nullptr);

    MaterialText2D mt2d(cfg);

    return mt2d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
