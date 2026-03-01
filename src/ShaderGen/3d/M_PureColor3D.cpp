#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/ShaderComposition.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include"S_PureColor3D.h"
#include"S_PureColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char mi_codes[]="vec4 Color;";          //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(Vector4f);     //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    Output.Color=mi.Color;

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

MaterialCreateInfo *CreatePureColor3D(const VulkanDevAttr *dev_attr,Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        PURE_COLOR_3D_DEF,
        PURE_COLOR_3D_COMPOSED_DEF,
        PURE_COLOR_3D_LOGIC,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[PureColor3D] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
