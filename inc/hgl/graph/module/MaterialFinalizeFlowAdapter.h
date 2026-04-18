#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <vector>
#include <cstdint>
#include <string>

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
        mtl::ShaderDataSchema mi_schema = mtl::ShaderDataSchema::None;
        std::string mi_schema_file;
    };

    void BuildMaterialFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::MaterialCreateInfo &mci,
                                   MaterialFinalizePlan &out_plan);
}
