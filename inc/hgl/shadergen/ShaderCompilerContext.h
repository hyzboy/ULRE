#pragma once

#include<hgl/shadergen/ShaderGenDiagnostic.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<string>
#include<vector>

namespace hgl::graph
{
struct ShaderBinary
{
    ShaderStage stage = ShaderStage::Vertex;
    std::vector<uint32_t> spirv;
};

struct ShaderCompileRequest
{
    ShaderStage stage = ShaderStage::Vertex;
    std::string source;
    mtl::contract::PhysicalDeviceProfileLite profile;
    uint32_t vulkan_version = 0;
    uint32_t spv_version = 0;
};

class ShaderCompilerContext
{
public:
    explicit ShaderCompilerContext(const mtl::contract::PhysicalDeviceProfileLite &profile);

    ShaderGenResult<ShaderBinary> Compile(const ShaderCompileRequest &request);

private:
    mtl::contract::PhysicalDeviceProfileLite profile;
};
}//namespace hgl::graph
