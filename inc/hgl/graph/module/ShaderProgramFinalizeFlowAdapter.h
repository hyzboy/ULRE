#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <vector>
#include <cstdint>

namespace hgl::graph
{
    class MaterialDescriptorManager;

    namespace mtl
    {
        class ShaderBuildContext;
    }

    struct ShaderProgramFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
    };

    void BuildShaderProgramFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const mtl::ShaderBuildContext &ctx,
                                   ShaderProgramFinalizePlan &out_plan);
}
