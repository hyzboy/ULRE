#pragma once

#include<hgl/shadergen/ShaderGenDiagnostic.h>
#include<hgl/shadergen/ShaderCompilerContext.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include<vector>

namespace hgl::graph
{
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

struct ShaderBuildResult
{
    ShaderBuildState final_state = ShaderBuildState::Empty;
    mtl::DescriptorBindingSlots binding_contract;
    uint32_t descriptor_count = 0;
    bool layout_finalized = false;
    std::vector<ShaderBinary> binaries;
    std::vector<ShaderGenDiagnostic> diagnostics;
};

class ShaderBuildPipeline
{
public:
    ShaderGenResult<ShaderBuildResult> Build(const mtl::MaterialCreateConfig &config,
                                             const mtl::contract::PhysicalDeviceProfileLite *profile);
};
}//namespace hgl::graph
