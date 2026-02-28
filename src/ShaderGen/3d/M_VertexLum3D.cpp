#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include"S_VertexLuminance3D.h"
#include"S_VertexLuminance3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="vec4 Color;";          //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(hgl::math::Vector4f);     //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    Output.Color=Luminance*mi.Color;

    gl_Position=GetPosition3D();
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=Input.Color;
})";// ^       ^
    // |       |
    // |       +--ps:这里的Input.Color就是上一个Shader中的Output.Color
    // +--ps:这里的Color就是最终的RT

    class MaterialVertexLuminance3D:public Std3DMaterial
    {
    public:

        using Std3DMaterial::Std3DMaterial;
        ~MaterialVertexLuminance3D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddInput(VAT_FLOAT,VAN::Luminance);

            vsc->AddOutput(SVT_VEC4,"Color");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(   mi_codes,                       //材质实例glsl代码
                                        mi_bytes,                       //材质实例数据大小
                                        (uint32_t)ShaderStage::Vertex);    //只在Vertex Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialVertexLuminance3D:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreateVertexLuminance3D(const VulkanDevAttr *dev_attr,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    ShaderPermutationKey key;
    const bool use_vec2_position = cfg && cfg->position_format.ToCode() == VAT_VEC2.ToCode();

    const FixedMaterialDef &fixed_def = use_vec2_position
        ? VERTEX_LUMINANCE_3D_DEF_VEC2
        : VERTEX_LUMINANCE_3D_DEF_VEC3;

    const ComposedMaterialDef &composed_def = use_vec2_position
        ? VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC2
        : VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC3;

    const MaterialLogicDef &logic_def = use_vec2_position
        ? VERTEX_LUMINANCE_3D_LOGIC_VEC2
        : VERTEX_LUMINANCE_3D_LOGIC;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        fixed_def,
        composed_def,
        logic_def,
        key,
        cfg);

    if (mci_new)
    {
        std::fprintf(stderr,
            "[VertexLuminance3D] using new composed-business compile path\n");
        return mci_new;
    }

    std::fprintf(stderr,
        "[VertexLuminance3D] composed-business compile failed, fallback to legacy Std3DMaterial path\n");

    MaterialVertexLuminance3D mvc3d(cfg);

    return mvc3d.Create(dev_attr);
}
}//namespace hgl::graph::mtl
