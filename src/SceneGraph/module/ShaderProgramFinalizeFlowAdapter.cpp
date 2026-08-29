#include <hgl/graph/module/ShaderProgramFinalizeFlowAdapter.h>
#include <hgl/vk/VKMaterialDescriptorManager.h>
#include <hgl/mtl/ShaderBuildContext.h>

namespace hgl::graph
{
    void BuildShaderProgramFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::ShaderBuildContext &ctx,
                                   ShaderProgramFinalizePlan &out_plan)
    {
        out_plan.mp_set_types.clear();
        (void)ctx;

        if (!desc_manager)
            return;

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; ++i)
        {
            const DescriptorSetType set_type = static_cast<DescriptorSetType>(i);
            if (desc_manager->hasSet(set_type))
                out_plan.mp_set_types.push_back(set_type);
        }
    }
}
