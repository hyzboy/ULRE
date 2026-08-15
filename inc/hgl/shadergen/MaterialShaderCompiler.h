#pragma once

namespace hgl::graph::mtl {}

/// MaterialShaderCompiler.h — canonical material input → ShaderBuildContext
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. Build DescriptorSetLayoutAllocator from the canonical descriptor input.
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 ShaderBuildContext*

#include <hgl/mtl/SerializedVertexEntry.h>
#include <hgl/mtl/SerializedDescriptorEntry.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/glsl/ShaderResourceManifest.h>
#include<hgl/shadergen/ShaderArtifactStore.h>
#include<hgl/shadergen/DescriptorContract.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    struct ShaderBufferSource;
    class GeometryVertexFormat;
}

namespace hgl::graph::shadergen{
    using namespace hgl::graph::mtl;
struct MaterialShaderCompilerInput
{
    const char *debug_name = nullptr;
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    const mtl::SerializedVertexEntry *vertex_entries = nullptr;
    uint32 vertex_entry_count = 0;
    const mtl::SerializedDescriptorEntry *descriptor_entries = nullptr;
    uint32 descriptor_entry_count = 0;
};

struct CompositorMaterialBuildConfig
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    uint32_t shader_stage_flag_bits = uint32_t(ShaderStage::VertexFragment);
    // Per-material SSBO slot declarations (index == data_slot).
    // When non-null and non-empty, MaterialShaderCompiler generates MaterialDataSlot
    // mtl::SerializedDescriptorEntry items and injects the material SSBO struct/buffer
    // declarations into the fragment GLSL.
    const std::vector<mtl::DataSlotDeclaration> *data_slot_decls = nullptr;
    // Optional: capability declaration source for development-time subset validation.
    // When non-null, CompileCompositorMaterial checks Layout requirements ⊆ Definition capabilities.
    const mtl::MaterialDefinition *material_definition = nullptr;
    // Optional unified stage/link contract. When supplied, the compiler
    // validates the declared VS/FS interface before compiling the local SPV.
    const ShaderLinkSpec *program_link = nullptr;
    const ShaderResourceManifest *resource_manifest = nullptr;
    bool merge_resource_manifest_material_slots = true;
    ShaderArtifactStore *artifact_store = nullptr;
    const DescriptorContract *descriptor_contract = nullptr;
    bool generate_only = false; // Preserve generated GLSL for contract tests without SPV compilation.
};

class ShaderBuildContext;

bool FinalizeShaderBuildContext(
    ShaderBuildContext *build_spec);

/**
 * 编译 Compositor 模板产出的完整 GLSL → ShaderBuildContext*。
 *
 * 使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param input     canonical compiler input
 * @param vs_glsl   完整的 vertex shader GLSL（含 #version, layout, main）
 * @param fs_glsl   完整的 fragment shader GLSL（含 #version, layout, main）
 * @param config    编译期配置视图
 * @return          编译好的 ShaderBuildContext*; 失败返回 nullptr
 */
ShaderBuildContext *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config);

}//namespace hgl::graph::shadergen
