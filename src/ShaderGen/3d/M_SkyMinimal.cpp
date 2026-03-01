#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/graph/mtl/MaterialCompiler.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include "S_SkyMinimal.h"
#include "S_SkyMinimal_Logic.h"
#include <cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateSkyMinimal(const VulkanDevAttr *dev_attr, const SkyMinimalCreateConfig *cfg)
{
    ShaderPermutationKey key;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        SKY_MINIMAL_DEF,
        SKY_MINIMAL_COMPOSED_DEF,
        SKY_MINIMAL_LOGIC,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[SkyMinimal] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
