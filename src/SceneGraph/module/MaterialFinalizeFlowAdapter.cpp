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
        out_plan.mi_data_bytes = mci.GetMaterialInstanceStride();
        out_plan.mi_max_count = mci.GetMaterialInstanceMaxCount();

        if (!desc_manager)
            return;

        for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; ++i)
        {
            const DescriptorSetType set_type = static_cast<DescriptorSetType>(i);
            if (desc_manager->hasSet(set_type))
                out_plan.mp_set_types.push_back(set_type);
        }

        bool has_vertex_data_set = false;
        for (const auto set_type : out_plan.mp_set_types)
        {
            if (set_type == DescriptorSetType::VertexData)
            {
                has_vertex_data_set = true;
                break;
            }
        }

        if (!has_vertex_data_set)
            out_plan.mp_set_types.push_back(DescriptorSetType::VertexData);
    }
}
