#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include"S_Gizmo3D.h"
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;

    if(cfg)
        cfg->material_instance=true;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        profile,
        GIZMO_3D_DEF,
        GIZMO_3D_COMPOSED_DEF,
        GIZMO_3D_LOGIC,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[Gizmo3D] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
