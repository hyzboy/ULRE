#include"Std2DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/2d/Material2DCreateConfig.h>
#include"common/MFRectPrimitive.h"
#include<hgl/graph/mtl/UBOCommon.h>

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

    constexpr const char gs_main[]=R"(
void main()
{
    vec2 vlt=gl_in[0].gl_Position.xy;
    vec2 vrb=gl_in[0].gl_Position.zw;
    vec2 tlt=Input[0].TexCoord.xy;
    vec2 trb=Input[0].TexCoord.zw;

    gl_Position=vec4(vlt,           vec2(0,1));Output.TexCoord=tlt;                EmitVertex();
    gl_Position=vec4(vlt.x, vrb.y,  vec2(0,1));Output.TexCoord=vec2(tlt.x,trb.y);  EmitVertex();
    gl_Position=vec4(vrb.x, vlt.y,  vec2(0,1));Output.TexCoord=vec2(trb.x,tlt.y);  EmitVertex();
    gl_Position=vec4(vrb,           vec2(0,1));Output.TexCoord=trb;                EmitVertex();

    EndPrimitive();
})";

    constexpr const char fs_main[]=R"(
void main()
{
    Color=texture(TextureColor,Input.TexCoord);
})";

    class MaterialRectTexture2D:public Std2DMaterial
    {
    public:

        using Std2DMaterial::Std2DMaterial;
        ~MaterialRectTexture2D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {            
            {
                RANGE_CHECK_RETURN_FALSE(cfg->coordinate_system)

                vsc->AddInput(VAT_VEC4,VAN::Position);

                vsc->AddFunction(func::GetPosition2DRect[size_t(cfg->coordinate_system)]);

                if(cfg->coordinate_system==CoordinateSystem2D::Ortho)
                {
                    mci->AddUBO(VK_SHADER_STAGE_VERTEX_BIT,
                                DescriptorSetType::Global,
                                SBS_ViewportInfo);
                }
            }

            vsc->AddInput(VAT_VEC4,VAN::TexCoord);

            vsc->AddOutput(VAT_VEC4,"TexCoord");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            gsc->SetGeom(Prim::Points,Prim::TriangleStrip,4);

            gsc->AddOutput(VAT_VEC2,"TexCoord");

            gsc->SetMain(gs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::Color);

            fsc->AddOutput(VAT_VEC4,"Color");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialRectTexture2D:public Std2DMaterial
}//namespace

MaterialCreateInfo *CreateRectTexture2D(mtl::Material2DCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->shader_stage_flag_bit|=VK_SHADER_STAGE_GEOMETRY_BIT;

    MaterialRectTexture2D mvc2d(cfg);

    return mvc2d.Create();
}
STD_MTL_NAMESPACE_END
