#pragma once

// MaterialDefinitionRegistry.h —— 材质定义的**数据面**入口（装载/查询/规范化）。
//
// 本头只描述「读材质定义 + 组装构建请求」这些运行时也要做的事，因此它可以被
// 发行形态（ULRE_ENABLE_SHADERGEN=0）单独编译。
//
// 生成期入口（CreateMaterialFromDefinition / ComputeMaterialProgramKey /
// BuildResolvedMaterialVertexABI）**不在这里**：它们要么发射 GLSL，要么要读
// ShaderLibrary/*.glsl 的声明闭包，只有生成器才知道怎么做。声明在
// inc/hgl/mtl/MaterialShaderCompiler.h（ShaderGen 专属头）。

#include<hgl/vk/VK.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/mtl/CanonicalShaderContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/RenderTemplate.h>
#include<hgl/mtl/ShaderProgramKey.h>
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
class ShaderArtifactStore;
namespace contract
{
    struct PhysicalDeviceProfileLite;
}

struct MaterialCoverageContract;
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
    // ambient provider 模块名（引擎/环境层决定，见 EnvironmentSystem::SetAmbientProvider）。
    // nullptr / 空串 = 用场景渲染模板默认 provider。
    // 与 recipe（材质数据）分开放：材质不再声明 ambient 算法。
    const char *ambient_module_name = nullptr;
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

// Render preparation: selects the scene render template from material data and
// overwrites the AmbientLightProvider slot with the engine-supplied
// `ambient_module_name` (never for depth/shadow purposes). The include path is
// NOT taken from configuration — the slot is left empty so that template
// validation canonicalizes it from the module registry.
//
// `ambient_module_name` 是**引擎/环境层**的选择（`EnvironmentSystem::SetAmbientProvider`
// → `EnvironmentManager::Profile::ambient_module_name`），不是材质数据：
// 环境光的算法与它的参数块（`ambient_env`）必须由同一个所有者配对。
// nullptr / 空串 = 保留模板默认 provider（`indirect_sky_ambient`）。
//
// Runtime (ShaderProgramManager) and the document/offline regression tools must
// share this single implementation; do not duplicate the routing rules.
bool SelectSceneRenderTemplateRequest(
    const MaterialDefinition &definition,
    ShaderProgramPurpose purpose,
    const MaterialRecipe &recipe,
    RenderTemplateRequest &out_request,
    const char *ambient_module_name = nullptr);

struct MaterialResolvedVertexABI
{
    VkFormat position_format = VK_FORMAT_UNDEFINED;
    AnsiString vertex_input_glsl;
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
        mtl::ShaderProgramPurpose::ForwardColor,
    // ambient provider 模块名（引擎/环境层选择）。它是**前向着色**的 program 身份维度：
    // 同一配方配不同 ambient 模块 ⇒ 不同 fragment 源码 ⇒ 必须落不同 program 身份。
    // depth purpose 不消费它（模板无该槽）⇒ 直接忽略，不白白分裂阴影变体。
    // nullptr / 空串 = 模板默认 provider，哈希取值与引入本维度之前**逐位相同**。
    const char *ambient_module_name = nullptr) noexcept;

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
 * 决定 —— 数据驱动表，不是散落的字面量。
 *   无参重载            = 非错误回退（`MaterialErrorKind::None`）⇒ 纯色 `builtin/pure_color`；
 *   错误种类            ⇒ 棋盘格 `builtin/checkerboard_3d`（2026-10-12 用户拍板：
 *                         材质配置写错只 LOG 报错，渲染走错误材质路径 = 棋盘格）。
 * 回退材质的硬约束（门 case `S.material-fallback-definitions-exist` 自动遍历本表校验）：
 * 定义必须已注册、顶点需求只依赖 Position、`provider_policy = GeometryOnly`
 * ⇒ 任意几何都画得出来。
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
