#pragma once

// MaterialDefinitionRegistry.h —— 材质定义的**数据面**入口（装载/查询/规范化）。
//
// 本头只描述「读材质定义 + 组装构建请求」这些运行时也要做的事，因此它可以被
// 发行形态（ULRE_ENABLE_SHADER_PROGRAM_BUILD=0）单独编译。
//
// 生成期入口（ComputeMaterialProgramKey / BuildResolvedMaterialVertexABI）**不在这里**：
// 它们要么发射 GLSL，要么要读 ShaderLibrary/*.glsl 的声明闭包，只有构建期才知道怎么做。
// 声明在 inc/hgl/mtl/StageSPVCompiler.h（阶段编译专用头）。

#include<hgl/vk/VK.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/mtl/CanonicalShaderContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/PassProfile.h>
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

// ── Layer 3: MaterialDefinitionBuildRequest = Build Context ──────────────────
// 描述"构建此帧 ShaderProgram 时的额外上下文"。包含 recipe（Layer 2）加上
// 构建期上下文（几何格式、Program Purpose、天光策略等）。
// 材质标识的唯一来源是 recipe（`domain` + `recipe_name`）；此结构不另持标识字段。
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
    // Render preparation supplies this resolved request. The program builder validates
    // and emits it but does not select scene lighting or quality policies.
    PassProfileRequest render_template_request;
};

// Render preparation calls this after selecting the scene template. It copies
// material capabilities into that request and structurally validates the
// completed request without selecting quality, scene, or provider policy.
bool AppendMaterialPassProfileRoots(
    const MaterialDefinition &definition,
    PassProfileRequest &request) noexcept;

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
bool SelectScenePassProfileRequest(
    const MaterialDefinition &definition,
    ShaderProgramPurpose purpose,
    const MaterialRecipe &recipe,
    PassProfileRequest &out_request,
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

/**
 * 由**配方**合成等效材质定义（配方直连领域；definition 文件层已删除）。
 *
 * 为什么还需要「定义」这个结构体：下游（顶点 ABI 求解、GLSL 拼装、pass profile
 * 选择、纹理引用布局）消费的是一份**已解析的完整契约**，它描述的正是「这个配方
 * 会生成怎样的管线」。这份结构体现由领域契约 + 配方当场算出，不存在第二处真源。
 *
 * 合成规则：
 *   id / name          = 领域名（人类可读标识；**同时**是 manifest key 与纹理池 key
 *                        的哈希输入，改取值等于改烘焙缓存身份）
 *   provider_policy    = contract->require_allow_derived ? AllowDerived : GeometryOnly
 *   vertex_node_config = contract 规范变换（配方 [transform] 仍优先，由 ABI 求解侧合并）
 *   vertex requirements= contract 规范语义集
 *   varyings           = contract 规范 mask（VertexPalette 另有 palette 特化）
 *   normal_mode        = contract 规范法线模式
 *   material_data      = contract 规范 SSBO 类型
 *   texture slots      = contract 白名单全集（槽的 sampler/required/channels 由契约给出）
 *   samplers / defines = 配方 `sampler_names` / `compile_defines` **整表替换**契约预设，
 *                        未声明则用 contract 惯例预设
 *   render_state       = contract 默认状态（配方覆盖项由 NormalizeRecipe 落定）
 *   模块名             = contract 规范模块；`surface_module_name` 取配方声明（Sky 的
 *                        surface_sky_cube 这类「同领域多表面形态」的唯一出口）
 *
 * 这是**唯一**的「配方 → 定义」入口：没有第二条按领域分支的路径，也没有持久化层。
 *
 * @return false = 配方未声明领域（`Custom`）或领域无契约。
 */
bool SynthesizeMaterialDefinitionFromRecipe(const MaterialRecipe &recipe,
                                            MaterialDefinition &out_definition) noexcept;

/**
 * 合成**回退（错误）材质**配方（A7b）：
 *   领域与根颜色都取自 `kFallbackMaterialRules`（表驱动），随后走既有的
 *   `NormalizeRecipe`（同一套契约默认值与渲染状态规范化）—— 因此回退配方与普通
 *   配方在下游**没有任何分支差异**，不需要兼容层。
 *
 * 回退材质的硬约束（门 case `S.material-fallback-domains-exist` 自动遍历规则表校验）：
 * 领域必须有契约、顶点需求只依赖 Position、`provider_policy = GeometryOnly`
 * ⇒ 任意几何都画得出来。
 *
 * @param kind `MaterialErrorKind::None` 时返回 false（调用方按“非材质错误”处理）。
 */
bool BuildFallbackMaterialRecipe(MaterialRecipe &out_recipe,
                                 const MaterialErrorKind kind);

/**
 * Normalize a MaterialRecipe in-place:
 *   1. 取配方 `domain` 的领域契约；无契约（`Custom` / 非法值）⇒ 记错误种类、
 *      整份改写成保底领域配方（`BuildFallbackMaterialRecipe`）。
 *   2. Applies contract defaults and resolved render state to the recipe.
 *
 * This is the canonical pre-processing step that must be called before the recipe is stored
 * in a primitive render path or passed to RenderSceneUBOSystem.  It is idempotent.
 */
void NormalizeRecipe(MaterialRecipe &recipe);

}//namespace hgl::graph::mtl
