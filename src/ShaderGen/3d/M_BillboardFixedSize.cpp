#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="uvec2 BillboardSize;";         //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(math::Vector2u);             //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    vec2 psize=vec2(mi.BillboardSize)/vec2(viewport.canvas_resolution);
    vec4 center_clip=camera.vp*GetLocalToWorld()*vec4(0.0,0.0,0.0,1.0);
    vec2 center_ndc=center_clip.xy/center_clip.w;
    vec2 ndc=center_ndc+Position.xy*psize;

    Output.TexCoord=vec2(Position.x+0.5,Position.y+0.5);

    gl_Position=vec4(ndc*center_clip.w,center_clip.z,center_clip.w);
})";

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=texture(TextureBaseColor,Input.TexCoord);
})";

    class MaterialBillboard2DFixedSize:public Std3DMaterial
    {
    public:

        MaterialBillboard2DFixedSize(mtl::BillboardMaterialCreateConfig *bcfg):Std3DMaterial(bcfg){}
        ~MaterialBillboard2DFixedSize()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

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

    cfg->local_to_world=true;

    cfg->material_instance=true;

    MaterialBillboard2DFixedSize mtl_billbard_2d_fixed_size(cfg);

    return mtl_billbard_2d_fixed_size.Create(dev_attr);
}
}//namespace hgl::graph::mtl
