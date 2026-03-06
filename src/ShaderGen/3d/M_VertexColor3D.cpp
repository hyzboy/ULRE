#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include"S_VertexColor3D.h"
#include"S_VertexColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        profile,
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
