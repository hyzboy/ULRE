#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/ShaderArtifactContract.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace contract
    {
        struct PhysicalDeviceProfileLite;
    }

    class ShaderBuildContext;

    bool BuildShaderProgramArtifactMetadata(
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderBuildContext &build_spec,
        ShaderProgramArtifactMetadata &out_metadata) noexcept;
}
