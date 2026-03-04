#pragma once

#include <hgl/vk/VKDescriptorSetType.h>
#include <vector>
#include <cstdint>

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
        uint32_t mi_data_bytes = 0;
        uint32_t mi_max_count = 0;
    };

    void BuildMaterialFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::MaterialCreateInfo &mci,
                                   MaterialFinalizePlan &out_plan);
}
