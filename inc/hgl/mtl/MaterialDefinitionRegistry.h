#pragma once

#include<hgl/vk/VK.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/mtl/CanonicalShaderContract.h>
#include<hgl/mtl/MaterialCoverageContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/GLSLCodeModuleCapabilityResolver.h>
#include<hgl/mtl/GLSLCodeModuleRegistry.h>
#include<hgl/mtl/SerializedVertexEntry.h>
#include<hgl/type/String.h>
#include<hgl/common/VertexAttribDef.h>
#include<hgl/common/RenderTargetOutputConfig.h>

namespace hgl::graph
{
    class GeometryVertexFormat;
    struct ShaderBufferSource;
}

namespace hgl::graph::mtl
{
class ShaderBuildContext;
class ShaderArtifactStore;
namespace contract
{
    struct PhysicalDeviceProfileLite;
}
}

namespace hgl::graph::mtl{

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
    mtl::ShaderArtifactStore *shader_artifact_store = nullptr;
    bool defer_finalize = false;  // 生成 GLSL 与契约后延迟 SPV 编译（生产主路径：先查缓存）
    bool override_shader_program_purpose = false;
    mtl::ShaderProgramPurpose shader_program_purpose =
        mtl::ShaderProgramPurpose::ForwardColor;

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
    mtl::ShaderProgramPurpose purpose,
    const mtl::MaterialCoverageContract &coverage) noexcept;

uint64 HashMaterialProgramBuildContext(
    PrimitiveType primitive_type,
    const GeometryVertexFormat *geometry_vertex_format,
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const mtl::ShaderProgramPurpose purpose =
        mtl::ShaderProgramPurpose::ForwardColor) noexcept;

mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const mtl::contract::PhysicalDeviceProfileLite *profile,
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
// C++ 硬编码材质已移除——所有材质定义（含内置 bootstrap）均为 TOML 文件承载。
bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_definition);
MaterialDefinitionFileRegistry &GetMaterialDefinitionFileRegistry();
GLSLCodeModuleRegistry &GetGLSLCodeModuleRegistry();

// ── built-in fallback definition ID 常量 ──────────────────────────────────────
// 缺材质安全网 = 纯色（原为独立 ID + alias 注册，alias 机制已删，常量直连）
constexpr const char *BUILTIN_MTL_DEF_MISSING_MATERIAL  = "builtin/pure_color";
constexpr const char *BUILTIN_MTL_DEF_TEXT              = "builtin/text";
constexpr const char *BUILTIN_MTL_DEF_TEXT_GPU          = "builtin/text_gpu";
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
    return BUILTIN_MTL_DEF_PURE_COLOR;
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
