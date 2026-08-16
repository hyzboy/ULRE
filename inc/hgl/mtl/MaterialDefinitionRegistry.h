#pragma once

#include<hgl/vk/VK.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/shadergen/CanonicalShaderContract.h>
#include<hgl/shadergen/MaterialCoverageContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/glsl/GLSLCodeModuleCapabilityResolver.h>
#include<hgl/graph/glsl/GLSLCodeModuleRegistry.h>
#include<hgl/mtl/SerializedVertexEntry.h>
#include<hgl/type/String.h>
#include<hgl/common/VertexAttribDef.h>
#include<hgl/common/RenderTargetOutputConfig.h>

namespace hgl::graph
{
    class GeometryVertexFormat;
    struct ShaderBufferSource;
}

namespace hgl::graph::shadergen
{
class ShaderBuildContext;
class ShaderArtifactStore;
namespace contract
{
    struct PhysicalDeviceProfileLite;
}
}

namespace hgl::graph::mtl{
    namespace shadergen = hgl::graph::shadergen;

class MaterialDefinitionFileRegistry;

// ── Layer 3: MaterialDefinitionBuildRequest = Build Context ──────────────────
// 描述"构建此帧 ShaderProgram 时的额外上下文"。包含 recipe（Layer 2）加上
// 构建期上下文（几何格式、Program Purpose、天光策略等）。
// recipe.mtl_def_id 是材质标识的唯一来源；此结构不再持有独立的 mtl_def_id 字段。
// ─────────────────────────────────────────────────────────────────────────────
struct MaterialDefinitionBuildRequest
{
    MaterialRecipe recipe;
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    const GeometryVertexFormat *geometry_vertex_format = nullptr;
    bool has_vertex_node_config_override = false;
    VertexShaderNodeConfig vertex_node_config_override;
    shadergen::ShaderArtifactStore *shader_artifact_store = nullptr;
    bool generate_only = false;
    bool override_shader_program_purpose = false;
    shadergen::ShaderProgramPurpose shader_program_purpose =
        shadergen::ShaderProgramPurpose::ForwardColor;

    const GLSLCodeModuleRegistry *vertex_code_module_registry = nullptr;

};

struct MaterialResolvedVertexABI
{
    VkFormat position_format = VK_FORMAT_UNDEFINED;
    uint64 provider_graph_hash = 0;
    ValueArray<SerializedVertexEntry> vertex_entries;
    AnsiString vertex_input_glsl;
    std::string provider_glsl;
};

VertexShaderNodeConfig ResolveMaterialVertexNodeConfig(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request) noexcept;

MaterialVertexVaryingConfig ResolveMaterialVertexVaryingConfig(
    const MaterialDefinition &definition,
    shadergen::ShaderProgramPurpose purpose,
    const shadergen::MaterialCoverageContract &coverage) noexcept;

uint64 HashMaterialProgramBuildContext(
    PrimitiveType primitive_type,
    const GeometryVertexFormat *geometry_vertex_format,
    const shadergen::contract::PhysicalDeviceProfileLite *profile) noexcept;

shadergen::ShaderBuildContext *CreateMaterialFromDefinition(
    const shadergen::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request);

/**
 * Resolve a definition's format-free vertex semantic contract against the
 * Geometry supplied for this build. This query has no effect on generated
 * GLSL, pipeline state, or caches.
 *
 * @return false only when there is no semantic contract or no Geometry format;
 *         otherwise `out_result.resolved` reports whether providers were found.
 */
bool PreviewMaterialVertexSemanticResolution(
    const GLSLCodeModuleRegistry &registry,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    GLSLCodeModuleResolutionResult &out_result);

/**
 * Build the resolver-derived vertex ABI without compiling shaders or mutating
 * a program/cache. This is the explicit Phase 4.4 switched-path payload.
 */
bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi);

VkFormat ResolveMaterialVertexSemanticFormat(const GeometryVertexFormat *gvf, VertexSemantic semantic, VkFormat fallback_format);
inline VkFormat ResolveMaterialPositionFormat(const GeometryVertexFormat *gvf, VkFormat fallback_format)
{
    return ResolveMaterialVertexSemanticFormat(gvf, VertexSemantic::Position, fallback_format);
}

// Material definition registry
void RegisterMaterialDefinition(const MaterialDefinition &definition);
// Compatibility-only route; aliases resolve to the canonical definition and
// never create a second MaterialDefinition registry entry.
void RegisterMaterialDefinitionAlias(const char *alias_id, const char *definition_id);
bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_definition);
bool TryGetMaterialDefinitionByBootstrapKind(const MaterialDefinitionBootstrapKind kind, MaterialDefinition &out_definition);
MaterialDefinitionFileRegistry &GetMaterialDefinitionFileRegistry();
GLSLCodeModuleRegistry &GetGLSLCodeModuleRegistry();

// ── built-in fallback definition ID 常量 ──────────────────────────────────────
constexpr const char *BUILTIN_MTL_DEF_FALLBACK          = "builtin/pure_color";
constexpr const char *BUILTIN_MTL_DEF_MISSING_MATERIAL  = "builtin/missing_material";
constexpr const char *BUILTIN_MTL_DEF_TEXT              = "builtin/text";
constexpr const char *BUILTIN_MTL_DEF_PURE_COLOR        = "builtin/pure_color";

inline bool IsPureColorMaterialDefinition(
    const MaterialDefinition &definition) noexcept
{
    return definition.bootstrap_kind == MaterialDefinitionBootstrapKind::PureColor;
}

inline bool IsBootstrapMaterialDefinition(
    const MaterialDefinition &definition) noexcept
{
    return definition.bootstrap_kind != MaterialDefinitionBootstrapKind::None;
}

inline const char *GetFallbackMaterialDefinitionID()
{
    return BUILTIN_MTL_DEF_FALLBACK;
}

/**
 * Normalize a MaterialRecipe in-place:
 *   1. Fills mtl_def_id and lod from the matched MaterialDefinition if they are unset.
 *   2. Applies definition defaults and resolved render state to the recipe.
 *
 * This is the canonical pre-processing step that must be called before the recipe is stored
 * in a PrimitiveComponent or passed to RenderDescriptorBindingSystem.  It is idempotent.
 */
void NormalizeRecipe(MaterialRecipe &recipe);

}//namespace hgl::graph::mtl
