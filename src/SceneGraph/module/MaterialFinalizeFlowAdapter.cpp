#include <hgl/graph/module/MaterialFinalizeFlowAdapter.h>
#include <hgl/vk/VKMaterialDescriptorManager.h>
#include <hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph
{
    void BuildMaterialFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::MaterialCreateInfo &mci,
                                   MaterialFinalizePlan &out_plan)
    {
        out_plan.mp_set_types.clear();
        out_plan.required_instance_layout = mtl::ResolveInstanceDataLayout(mci.GetMaterialInstanceStride());
        out_plan.instance_max_count = mci.GetMaterialInstanceMaxCount();

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
