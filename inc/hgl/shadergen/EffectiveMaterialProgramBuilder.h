#pragma once

#include <hgl/mtl/MaterialProgramContract.h>

namespace hgl::graph::mtl
{
    struct MaterialDefinition;
    struct MaterialDefinitionBuildRequest;
    struct ShaderProgramLinkSpec;

    bool BuildEffectiveMaterialProgram(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const ShaderProgramLinkSpec &program_link,
        EffectiveMaterialProgramKey &out_effective_program,
        MaterialResolutionResult &out_resolution) noexcept;
}
