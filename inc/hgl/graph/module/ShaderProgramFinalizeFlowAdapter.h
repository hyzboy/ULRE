#pragma once

#include <hgl/common/DescriptorSetTypeDef.h>
#include <vector>
#include <cstdint>

namespace hgl::graph
{
    class MaterialDescriptorManager;

    namespace shadergen
    {
        class ShaderBuildContext;
    }

    struct ShaderProgramFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
    };

    void BuildShaderProgramFinalizePlan(const MaterialDescriptorManager *desc_manager,
                                   const shadergen::ShaderBuildContext &ctx,
                                   ShaderProgramFinalizePlan &out_plan);
}
