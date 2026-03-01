#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include"S_VertexColor3D.h"
#include"S_VertexColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char vs_main[]=R"(
void main()
{
    Output.Color=Color;

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

MaterialCreateInfo *CreateVertexColor3D(const VulkanDevAttr *dev_attr,const Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        VERTEX_COLOR_3D_DEF,
        VERTEX_COLOR_3D_COMPOSED_DEF,
        VERTEX_COLOR_3D_LOGIC,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[VertexColor3D] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
