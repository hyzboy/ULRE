#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include"common/MFRectPrimitive.h"
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/mtl/SamplerName.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char mi_codes[]="uvec4 id;";          //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(Vector4u);       //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    HandoverMI();
    Output.TexCoord=TexCoord;

    gl_Position=GetPosition2D();
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

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
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT,DescriptorSetType::PerMaterial,SamplerType::Sampler2DArray,mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(   mi_codes,                       //材质实例glsl代码
                                        mi_bytes,                       //材质实例数据大小
                                        VK_SHADER_STAGE_FRAGMENT_BIT);  //只在Fragment Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialRectTexture2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreateRectTexture2DArray(const VulkanDevAttr *dev_attr,mtl::Material2DCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->shader_stage_flag_bit|=VK_SHADER_STAGE_GEOMETRY_BIT;

    MaterialRectTexture2D mvc2d(cfg);

    return mvc2d.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
