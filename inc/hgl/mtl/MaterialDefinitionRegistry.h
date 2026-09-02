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
    bool override_shader_program_purpose = false;
    mtl::ShaderProgramPurpose shader_program_purpose =
        mtl::ShaderProgramPurpose::ForwardColor;
    // ECS render preparation will populate this once template composition
    // replaces the current material-definition shader selection path.
    RenderTemplateRequest render_template_request;

};

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

// Material definition registry
// 所有材质定义（含内置 bootstrap）均为 TOML 文件承载。
bool TryGetMaterialDefinitionByID(const std::string &mtl_def_id, MaterialDefinition &out_definition);
MaterialDefinitionFileRegistry &GetMaterialDefinitionFileRegistry();

// ── built-in fallback definition ID 常量 ──────────────────────────────────────
// 缺材质安全网 = 纯色
constexpr const char *BUILTIN_MTL_DEF_MISSING_MATERIAL  = "builtin/pure_color";
constexpr const char *BUILTIN_MTL_DEF_TEXT              = "builtin/text_gpu";
constexpr const char *BUILTIN_MTL_DEF_TEXT_BITMAP       = "builtin/text_gpu_bitmap";
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
