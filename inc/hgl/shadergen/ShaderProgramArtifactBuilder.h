#pragma once

#include <hgl/shadergen/ShaderArtifactContract.h>

namespace hgl::graph::mtl
{
    namespace contract
    {
        struct PhysicalDeviceProfileLite;
    }

    class ShaderProgramBuildSpec;

    bool BuildShaderProgramArtifactMetadata(
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderProgramBuildSpec &build_spec,
        ShaderProgramArtifactMetadata &out_metadata) noexcept;
}
