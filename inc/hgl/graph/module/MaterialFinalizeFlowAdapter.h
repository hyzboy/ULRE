#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <vector>
#include <cstdint>

namespace hgl::graph
{
    class MaterialDescriptorManager;

    namespace mtl
    {
        class ShaderProgramBuildSpec;
    }

    struct MaterialFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
    };

    void BuildMaterialFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::ShaderProgramBuildSpec &mci,
                                   MaterialFinalizePlan &out_plan);
}
