#pragma once

#include<hgl/shadergen/ShaderGenDiagnostic.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include<vector>

namespace hgl::graph
{
struct ShaderBuildResult
{
    std::vector<ShaderGenDiagnostic> diagnostics;
};

enum class ShaderBuildState
{
    Empty,
    ConfigValidated,
    SpecBuilt,
    DescriptorLayoutFinalized,
    SourceGenerated,
    Compiled,
    Failed
};

class ShaderBuildPipeline
{
public:
    ShaderGenResult<ShaderBuildResult> Build(const mtl::MaterialCreateConfig &config,
                                             const mtl::contract::PhysicalDeviceProfileLite *profile);
};
}//namespace hgl::graph
