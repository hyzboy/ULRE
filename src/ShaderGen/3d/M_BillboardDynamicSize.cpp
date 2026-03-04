#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    vec3 center = (GetLocalToWorld() * vec4(0.0,0.0,0.0,1.0)).xyz;
    vec3 world_pos = center
                   + Position.x * camera.billboard_right
                   + Position.y * camera.billboard_up;

    Output.TexCoord=vec2(Position.x+0.5,Position.y*-1.0+0.5);

    gl_Position = camera.vp * vec4(world_pos,1.0);
})";

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=texture(TextureBaseColor,Input.TexCoord);
})";

    class MaterialBillboard2DDynamicSize:public Std3DMaterial
    {
    public:

        MaterialBillboard2DDynamicSize(mtl::BillboardMaterialCreateConfig *bcfg):Std3DMaterial(bcfg){}
        ~MaterialBillboard2DDynamicSize()=default;

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
    };//class MaterialBillboard2DDynamicSize:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateBillboard2DDynamic(const contract::PhysicalDeviceProfileLite *profile,mtl::BillboardMaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    cfg->local_to_world=true;

    MaterialBillboard2DDynamicSize mtl_billbard_2d(cfg);

    return mtl_billbard_2d.Create(profile);
}
}//namespace hgl::graph::mtl
