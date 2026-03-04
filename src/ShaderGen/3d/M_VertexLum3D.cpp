#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include"S_VertexLuminance3D.h"
#include"S_VertexLuminance3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
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
        profile,
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
