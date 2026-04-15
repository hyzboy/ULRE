#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/mtl/InstanceDataLayout.h>
#include <vector>

namespace hgl::graph
{
    class MaterialDescriptorManager;

    namespace mtl
    {
        class MaterialCreateInfo;
    }

    struct MaterialFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
        mtl::InstanceDataLayout required_instance_layout = mtl::InstanceDataLayout::None;
        uint32_t instance_max_count = 0;
    };

    void BuildMaterialFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::MaterialCreateInfo &mci,
                                   MaterialFinalizePlan &out_plan);
}
