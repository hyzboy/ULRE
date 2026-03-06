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
#include<hgl/mtl/UBOCommon.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include"S_VertexPattleColor3D.h"
#include"S_VertexPattleColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexPattleColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    static const ShaderBufferSource * const private_sbs_list[] =
    {
        &SBS_ColorPattle
    };
    local_cfg.SetPrivateShaderBufferSources(private_sbs_list,1);

    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        profile,
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
