#pragma once

#include<hgl/vk/VK.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/mtl/CanonicalShaderContract.h>
#include<hgl/mtl/MaterialCoverageContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/RenderTemplate.h>
#include<hgl/mtl/ShaderCodeModuleCapabilityResolver.h>
#include<hgl/mtl/ShaderCodeModuleRegistry.h>
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
struct MaterialShaderDocumentCapture;
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
    mtl::ShaderProgramPurpose shader_program_purpose =
        mtl::ShaderProgramPurpose::ForwardColor;
    // Render preparation supplies this resolved request. ShaderGen validates
    // and emits it but does not select scene lighting or quality policies.
    RenderTemplateRequest render_template_request;

};

// Render preparation calls this after selecting the scene template. It copies
// material capabilities into that request and structurally validates the
// completed request without selecting quality, scene, or provider policy.
bool AppendMaterialRenderTemplateRoots(
    const MaterialDefinition &definition,
    RenderTemplateRequest &request) noexcept;

struct MaterialResolvedVertexABI
{
    VkFormat position_format = VK_FORMAT_UNDEFINED;
    uint64 provider_graph_hash = 0;
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

// Document capture is diagnostic-only. Keep it out of the frequently passed
// build request so adding observability does not change that request's ABI.
mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const mtl::contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    mtl::MaterialShaderDocumentCapture *document_capture);

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

// Material definition registry. Every definition is loaded from a schema-3
// TOML file; there is no native material-definition fallback.
bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_definition);
MaterialDefinitionFileRegistry &GetMaterialDefinitionFileRegistry();

/**
 * 回退（错误）材质的**定义 ID**：由 `kFallbackMaterialRules`（MaterialRecipe.h）唯一
 * 决定 —— 数据驱动表，不是散落的字面量。无参重载 = 非错误回退
 * （`MaterialErrorKind::None`），返回值仍是 `builtin/pure_color`。
 */
inline const char *GetFallbackMaterialDefinitionID(const MaterialErrorKind kind)
{
    return GetFallbackMaterialRule(kind).definition_id;
}

inline const char *GetFallbackMaterialDefinitionID()
{
    return GetFallbackMaterialDefinitionID(MaterialErrorKind::None);
}

/**
 * 合成**回退（错误）材质**配方（A7b）：
 *   定义 ID 与根颜色都取自 `kFallbackMaterialRules`（表驱动），随后走既有的
 *   `NormalizeRecipe`（同一套定义默认值与渲染状态规范化）—— 因此回退配方与普通
 *   配方在下游**没有任何分支差异**，不需要兼容层。
 *
 * @param kind `MaterialErrorKind::None` 时返回 false（调用方按“非材质错误”处理）。
 */
bool BuildFallbackMaterialRecipe(MaterialRecipe &out_recipe,
                                 const MaterialErrorKind kind);

/**
 * Normalize a MaterialRecipe in-place:
 *   1. Fills mtl_def_id from the matched MaterialDefinition if it is unset.
 *   2. Applies definition defaults and resolved render state to the recipe.
 *
 * This is the canonical pre-processing step that must be called before the recipe is stored
 * in a primitive render path or passed to RenderSceneUBOSystem.  It is idempotent.
 */
void NormalizeRecipe(MaterialRecipe &recipe);

}//namespace hgl::graph::mtl
