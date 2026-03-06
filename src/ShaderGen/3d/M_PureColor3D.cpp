#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include"S_PureColor3D.h"
#include"S_PureColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;
    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        profile,
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
