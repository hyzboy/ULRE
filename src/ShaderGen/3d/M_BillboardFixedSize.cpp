#include"Std3DMaterial.h"
#include"S_BillboardVertex.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN
namespace
{
    constexpr const char mi_codes[]="uvec2 BillboardSize;";         //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(Vector2u);             //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    Output.BillboardSize=vec2(mi.BillboardSize)/vec2(viewport.canvas_resolution);

    gl_Position=GetPosition3D();
    gl_Position/=gl_Position.w;
})";

    constexpr const char gs_main[]=R"(
void main()
{
    for(int i=0;i<4;i++)
    {
        gl_Position=gl_in[0].gl_Position;
        gl_Position.xy+=BillboardVertex[i]*Input[0].BillboardSize;

        Output.TexCoord=BillboardVertex[i]+vec2(0.5);

        EmitVertex();
    }
    EndPrimitive();
})";

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=texture(TextureBaseColor,Input.TexCoord);
})";

    class MaterialBillboard2DFixedSize:public Std3DMaterial
    {
        mtl::BillboardMaterialCreateConfig *billboard_config;

    public:

        MaterialBillboard2DFixedSize(mtl::BillboardMaterialCreateConfig *bcfg):Std3DMaterial(bcfg)
        {
            billboard_config=bcfg;
        }
        ~MaterialBillboard2DFixedSize()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddOutput(SVT_VEC2,"BillboardSize");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            gsc->SetGeom(PrimitiveType::Points,PrimitiveType::TriangleStrip,4);

            gsc->AddOutput(SVT_VEC2,"TexCoord");

            gsc->AddUserData(billboard_config->front_face==VK_FRONT_FACE_CLOCKWISE?shader_billboard_vertex_cw:shader_billboard_vertex_ccw);
            gsc->SetMain(gs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            mci->AddTextureSampler(ShaderStage::Fragment,DescriptorSetType::PerMaterial,SamplerType::Sampler2D,mtl::SamplerName::BaseColor);

            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(mi_codes,                       //材质实例glsl代码
                                     mi_bytes,                       //材质实例数据大小
                                     (uint32_t)ShaderStage::Vertex);    //只在Vertex Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialBillboard2DFixedSize:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateBillboard2DFixedSize(const VulkanDevAttr *dev_attr,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->enableGeometryShader();

    cfg->local_to_world=true;

    MaterialBillboard2DFixedSize mtl_billbard_2d_fixed_size(cfg);

    return mtl_billbard_2d_fixed_size.Create(dev_attr);
}
STD_MTL_NAMESPACE_END
