#pragma once

namespace hgl::graph::mtl {}

#include <hgl/shadergen/ShaderArtifactContract.h>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
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
