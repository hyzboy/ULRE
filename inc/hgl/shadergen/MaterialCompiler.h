#pragma once

/// MaterialCompiler.h — canonical material input → ShaderProgramBuildSpec
///
/// 使用 CompileCompositorMaterial 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. Build MaterialDescriptorInfo from the canonical descriptor input.
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 ShaderProgramBuildSpec*

#include <hgl/mtl/FixedVertexEntry.h>
#include <hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/mtl/SkyLight.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/glsl/ShaderResourceManifest.h>
#include<hgl/shadergen/ShaderArtifactStore.h>
#include<hgl/shadergen/MaterialDescriptorContract.h>
#include <hgl/mtl/MaterialProgramContract.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    struct ShaderBufferSource;
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl{

struct MaterialCompilerInput
{
    const char *debug_name = nullptr;
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    const FixedVertexEntry *vertex_entries = nullptr;
    uint32 vertex_entry_count = 0;
    const FixedDescriptorEntry *descriptor_entries = nullptr;
    uint32 descriptor_entry_count = 0;
};

struct CompositorMaterialBuildConfig
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    uint32_t shader_stage_flag_bits = uint32_t(ShaderStage::VertexFragment);
    // Per-material SSBO slot declarations (index == data_slot).
    // When non-null and non-empty, MaterialCompiler generates MaterialDataSlot
    // FixedDescriptorEntry items and injects the material SSBO struct/buffer
    // declarations into the fragment GLSL.
    const std::vector<MaterialDataSlotDecl> *data_slot_decls = nullptr;
    // Optional: capability declaration source for development-time subset validation.
    // When non-null, CompileCompositorMaterial checks Layout requirements ⊆ Definition capabilities.
    const MaterialDefinition *material_definition = nullptr;
    // Optional unified stage/link contract. When supplied, the compiler
    // validates the declared VS/FS interface before compiling the local SPV.
    const ShaderProgramLinkSpec *program_link = nullptr;
    const ShaderResourceManifest *resource_manifest = nullptr;
    bool merge_resource_manifest_material_slots = true;
    ShaderArtifactStore *artifact_store = nullptr;
    const MaterialDescriptorContract *descriptor_contract = nullptr;
    const EffectiveMaterialProgramKey *effective_program = nullptr;
    const MaterialResolutionResult *material_resolution = nullptr;
    const MaterialRecipe *material_recipe = nullptr;
    bool generate_only = false; // Preserve generated GLSL for contract tests without SPV compilation.
};

class ShaderProgramBuildSpec;

bool FinalizeShaderProgramBuildSpec(
    ShaderProgramBuildSpec *build_spec);

/**
 * 编译 Compositor 模板产出的完整 GLSL → ShaderProgramBuildSpec*。
 *
 * 使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param input     canonical compiler input
 * @param vs_glsl   完整的 vertex shader GLSL（含 #version, layout, main）
 * @param fs_glsl   完整的 fragment shader GLSL（含 #version, layout, main）
 * @param config    编译期配置视图
 * @return          编译好的 ShaderProgramBuildSpec*; 失败返回 nullptr
 */
ShaderProgramBuildSpec *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialCompilerInput &input,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const CompositorMaterialBuildConfig &config);

}//namespace hgl::graph::mtl
