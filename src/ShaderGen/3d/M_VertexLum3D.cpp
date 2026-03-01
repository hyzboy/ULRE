#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
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

    const MaterialLogicDef &logic_def = VERTEX_LUMINANCE_3D_LOGIC;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        fixed_def,
        composed_def,
        logic_def,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[VertexLuminance3D] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
