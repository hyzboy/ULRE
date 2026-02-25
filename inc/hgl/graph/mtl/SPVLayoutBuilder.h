#pragma once
/**
 * SPVLayoutBuilder.h — Layer 3: Build Vulkan objects from SPIR-V reflection
 *
 * This module converts SPVParseData (produced by GLSLCompiler.dll) into
 * concrete Vulkan objects and validation results.  It is the single place
 * in the engine that bridges the compiler layer and the renderer layer.
 *
 * Design document: doc/refactor/ShaderGen_Compiler_Loader_Separation.md
 *
 * Usage:
 *   // After compiling GLSL and obtaining SPVParseData:
 *   auto dsl   = BuildDescriptorSetLayout(device, vert_parse, frag_parse);
 *   auto attrs = BuildVertexInputAttributes(vert_parse);
 *   auto check = ValidateStageIO(vert_parse, frag_parse);
 */

#include <hgl/graph/mtl/SPVParseData.h>
#include <hgl/vk/VK.h>
#include <string>
#include <vector>

namespace hgl::graph
{

class VulkanDevice;
class MaterialDescriptorInfo;

// ---------------------------------------------------------------------------
// VkFormat derivation helper
// ---------------------------------------------------------------------------

/// Convert a SPIR-V scalar type + vector size to the matching VkFormat.
/// Returns VK_FORMAT_UNDEFINED for unsupported combinations.
VkFormat SPVAttribToVkFormat(SPVBaseType bt, uint8_t vec_size);

// ---------------------------------------------------------------------------
// Descriptor set layout
// ---------------------------------------------------------------------------

/// Build a VkDescriptorSetLayout by merging the descriptor bindings from two
/// (or three) shader stages.  Stage visibility flags are set automatically:
///   - binding seen in vert_only  → VK_SHADER_STAGE_VERTEX_BIT
///   - binding seen in frag_only  → VK_SHADER_STAGE_FRAGMENT_BIT
///   - binding seen in both       → VK_SHADER_STAGE_VERTEX_BIT | FRAGMENT_BIT
/// Bindings with the same (set, binding) must have the same kind; a mismatch
/// returns nullptr and writes a message to stderr.
VkDescriptorSetLayout BuildDescriptorSetLayout(
    VulkanDevice            *device,
    const SPVParseData      *vert_parse,
    const SPVParseData      *frag_parse,
    const SPVParseData      *geom_parse = nullptr);

// ---------------------------------------------------------------------------
// Vertex input attribute descriptions
// ---------------------------------------------------------------------------

struct VertexAttributeDesc
{
    uint32_t    location;
    VkFormat    format;
    std::string name;       ///< semantic name from SPIR-V (e.g. "Position", "Normal")
};

/// Extract vertex input attribute descriptions from the vertex stage reflection.
/// Returns one entry per stage_input, ordered by location.
/// The caller uses these to match against VertexArrayBuffer channels.
std::vector<VertexAttributeDesc> BuildVertexInputAttributes(
    const SPVParseData *vert_parse);

// ---------------------------------------------------------------------------
// Stage IO cross-validation (vert outputs ↔ frag inputs)
// ---------------------------------------------------------------------------

struct IOValidationResult
{
    bool        ok = true;
    std::string error;  ///< human-readable mismatch description
};

/// Verify that every fragment-stage input location is satisfied by a
/// corresponding vertex-stage output with the same location and compatible type.
/// Should be called in DEBUG builds; logs to stderr on failure.
IOValidationResult ValidateStageIO(
    const SPVParseData *vert_parse,
    const SPVParseData *frag_parse);

// ---------------------------------------------------------------------------
// Generator intent vs. compiler reflection cross-check
// ---------------------------------------------------------------------------

struct DescriptorConsistencyResult
{
    bool        ok = true;
    std::string error;  ///< first mismatch found
};

/// Compare what the generator declared (MaterialDescriptorInfo) against what
/// SPIR-V reflection actually found.  Call in DEBUG builds only; discrepancies
/// mean the GLSL layout declarations and the engine's descriptor tracking have
/// diverged.
DescriptorConsistencyResult ValidateDescriptorConsistency(
    const MaterialDescriptorInfo *gen_decl,
    const SPVParseData           *vert_parse,
    const SPVParseData           *frag_parse);

// ---------------------------------------------------------------------------
// Vertex input presence check (GPU-Driven invariants)
// ---------------------------------------------------------------------------

struct GPUDrivenInputCheck
{
    bool    has_l2w_id    = false;  ///< L2W_ID (uint, instanced)
    bool    has_mi_id     = false;  ///< MI_ID  (uint, instanced)
    bool    ok            = false;  ///< true iff both IDs found with expected type
    std::string error;
};

/// Verify that the vertex shader inputs contain the L2W_ID and MI_ID
/// instanced attributes required by the "one DrawCall" GPU-Driven architecture.
GPUDrivenInputCheck ValidateGPUDrivenInputs(const SPVParseData *vert_parse);

} // namespace hgl::graph
