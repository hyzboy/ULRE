#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <vector>
#include <cstdint>

namespace hgl::graph
{
    class MaterialDescriptorManager;

    namespace shadergen
    {
        class ShaderProgramBuildSpec;
    }

    struct ShaderProgramFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
    };

    void BuildShaderProgramFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const shadergen::ShaderProgramBuildSpec &mci,
                                   ShaderProgramFinalizePlan &out_plan);
}
