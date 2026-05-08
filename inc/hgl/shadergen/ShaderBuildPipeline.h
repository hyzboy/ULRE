#pragma once

#include<hgl/shadergen/ShaderGenDiagnostic.h>
#include<hgl/shadergen/ShaderCompilerContext.h>
#include<hgl/shadergen/MaterialBuilderBlocks.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/MaterialCreateConfig.h>
#include<hgl/mtl/StaticMaterialDef.h>
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

struct ShaderBuildDescriptorSpec
{
    std::vector<mtl::UBODescriptorSemantic> ubos;
    std::vector<mtl::SSBODescriptorSemantic> ssbos;
    uint32_t material_instance_bytes = 0;
    mtl::ShaderDataSchema material_instance_schema = mtl::ShaderDataSchema::None;
};

struct ShaderBuildResult
{
    ShaderBuildState final_state = ShaderBuildState::Empty;
    mtl::DescriptorBindingSlots binding_contract;
    mtl::MaterialInstanceBlock material_instance;
    mtl::LocalToWorldBlock local_to_world;
    uint32_t descriptor_count = 0;
    bool layout_finalized = false;
    std::vector<ShaderBinary> binaries;
    std::vector<ShaderGenDiagnostic> diagnostics;
};

class ShaderBuildPipeline
{
public:
    ShaderGenResult<ShaderBuildResult> Build(const mtl::MaterialCreateConfig &config,
                                             const mtl::contract::PhysicalDeviceProfileLite *profile,
                                             const ShaderBuildDescriptorSpec *descriptor_spec=nullptr);

    ShaderGenResult<mtl::MaterialCreateInfo *> BuildMaterialCreateInfo(
        const mtl::StaticMaterialDef &def,
        const mtl::MaterialCreateConfig &config,
        const mtl::contract::PhysicalDeviceProfileLite *profile,
        const std::string &vs_glsl,
        const std::string &fs_glsl);
};
}//namespace hgl::graph
