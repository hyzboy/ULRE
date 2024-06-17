#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.l2w_id=Assign.x;
    gl_Position=vec4(Position,1);
})";

    constexpr const char gs_main[]=R"(
void main()
{
    const vec2 BillboardVertex[4]=vec2[]
    (
        vec2(-0.5,-0.5),
        vec2( 0.5,-0.5),
        vec2(-0.5, 0.5),
        vec2( 0.5, 0.5)
    );

    for(int i=0;i<4;i++)
    {
        gl_Position=camera.vp
                    *l2w.mats[Input[0].l2w_id]
                    *vec4(  gl_in[0].gl_Position.xyz+
                            BillboardVertex[i].x*camera.billboard_right+
                            BillboardVertex[i].y*camera.billboard_up,
                            1
                         );

        Output.TexCoord=vec2(BillboardVertex[i].x+0.5,BillboardVertex[i].y*-1.0+0.5);

        EmitVertex();
    }
    EndPrimitive();
})";
    
        constexpr const char fs_main[]=R"(
void main()
{
    FragColor=texture(TextureBaseColor,Input.TexCoord);
})";

    class MaterialBillboard2DDynamicSize:public Std3DMaterial
    {
    public:

        using Std3DMaterial::Std3DMaterial;
        ~MaterialBillboard2DDynamicSize()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddOutput(VAT_UINT,"l2w_id",Interpolation::Flat);

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
            mci->AddSampler(VK_SHADER_STAGE_FRAGMENT_BIT,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }
    };//class MaterialBillboard2DDynamicSize:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateBillboard2DDynamic(mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);
    
    cfg->shader_stage_flag_bit|=VK_SHADER_STAGE_GEOMETRY_BIT;

    cfg->local_to_world=true;

    MaterialBillboard2DDynamicSize mtl_billbard_2d(cfg);

    return mtl_billbard_2d.Create();
}
STD_MTL_NAMESPACE_END
