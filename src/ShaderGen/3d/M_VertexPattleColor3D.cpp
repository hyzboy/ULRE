/** 顶点调色板色要求有一个UBO结构如下
*
*
*   struct ColorPattle
*   {
*       vec4 color[256];
*   }color_pattle;
*
*   然后输入的一个R8UI顶点属性来指定使用那个颜色。
*/

#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include"S_VertexPattleColor3D.h"
#include"S_VertexPattleColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.Color=color_pattle.color[Color];  //Color是输入的R8UI顶点属性

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

MaterialCreateInfo *CreateVertexPattleColor3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    static const ShaderBufferSource * const private_sbs_list[] =
    {
        &SBS_ColorPattle
    };
    local_cfg.SetPrivateShaderBufferSources(private_sbs_list,1);

    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        VERTEX_PATTLE_COLOR_3D_DEF,
        VERTEX_PATTLE_COLOR_3D_COMPOSED_DEF,
        VERTEX_PATTLE_COLOR_3D_LOGIC,
        key,
        &local_cfg);

    if (!mci_new)
        std::fprintf(stderr, "[VertexPattleColor3D] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
