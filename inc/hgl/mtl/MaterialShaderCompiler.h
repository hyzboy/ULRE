#pragma once

namespace hgl::graph::mtl {}

/// MaterialShaderCompiler.h — canonical material input → ShaderBuildContext
///
/// 使用 BuildMaterialStageDocuments 编译 Compositor 模板产出的完整 GLSL。
/// 内部流程：
///   1. 求解层构建契约/schema（描述符分配器已随 BDA 化退役）
///   2. 使用 SetFinalGLSL + CreateShaderDirect 直接编译
///   3. 填充并返回 ShaderBuildContext*

#include<hgl/common/ShaderStageDef.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/ShaderCodeResourceManifest.h>
#include<hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/ShaderDocument.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    struct ShaderBufferSource;
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl{
struct MaterialShaderDocumentCapture
{
    // Optional production-pipeline observability for regression gates. The
    // compiler serializes final documents from these exact instances.
    //
    // The capture object is intentionally a snapshot-only observer: it is reset
    // before production generation and populated only after a stage document has
    // been fully authored/validated. This keeps the diagnostic flow out of the
    // real generation path and avoids hidden state drift on the cache key path.
    ShaderDocument mesh_source_document;
    ShaderDocument fragment_document;
    ShaderDocument mesh_final_document;
    ShaderDocument fragment_final_document;

    void Clear() noexcept
    {
        mesh_source_document.Clear();
        fragment_document.Clear();
        mesh_final_document.Clear();
        fragment_final_document.Clear();
    }

    void CaptureSourceDocuments(
        const ShaderDocument &mesh_source,
        const ShaderDocument &fragment_source) noexcept
    {
        const ShaderDocument mesh_source_snapshot = mesh_source;
        const ShaderDocument fragment_source_snapshot = fragment_source;
        mesh_source_document = mesh_source_snapshot;
        fragment_document = fragment_source_snapshot;
    }

};

struct MaterialShaderCompilerInput
{
    const char *debug_name = nullptr;
    PrimitiveType primitive_type = PrimitiveType::Triangles;
};

struct MaterialCompileConfig
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    uint32_t shader_stage_flag_bits = uint32_t(ShaderStage::MeshFragment);
    // Per-material SSBO 单一声明（名称为 DefaultMaterialPrivateDataName）。
    // Material payloads are tracked with the material-specific enum, not the generic
    // runtime SSBO enum.
    GlobalSSBOType material_private_data = GlobalSSBOType::PBRSurface;
    // Optional: definition view for texture-reference validation + 运行时数据行需求直判。
    const mtl::MaterialDefinition *material_definition = nullptr;
    // Optional unified stage/link contract. When supplied, the compiler
    // validates the declared VS/FS interface before compiling the local SPV.
    const ShaderLinkSpec *program_link = nullptr;
    const ShaderCodeResourceManifest *resource_manifest = nullptr;
    ShaderArtifactStore *artifact_store = nullptr;
    bool defer_finalize = false; // 生成 GLSL/契约后返回，SPV 编译推迟（生产主路径先查 program 缓存）
};

class ShaderBuildContext;

bool FinalizeShaderBuildContext(
    ShaderBuildContext *build_spec);

// ── 生成期入口（定义 → 构建上下文 / 声明键 / 顶点 ABI）────────────────────────
// 这些函数**只属于生成器**：要么发射 GLSL，要么读 ShaderLibrary/*.glsl 的声明闭包。
// 它们原先声明在 MaterialDefinitionRegistry.h（数据面）里，会把「发行形态用不到的
// 生成期能力」挂在数据面的头文件上；现在收到本头，数据面只留纯数据能力。
// 实现位置：ShaderGenProgramBuildBackend.cpp / GenericMaterialBuilder.cpp /
// VertexABIBuilder.cpp。

mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request);

// Document capture is diagnostic-only. Keep it out of the frequently passed
// build request so adding observability does not change that request's ABI.
mtl::ShaderBuildContext *CreateMaterialFromDefinition(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialShaderDocumentCapture *document_capture);

/**
 * Build the resolver-derived vertex ABI without compiling shaders or mutating
 * a program/cache. This is the explicit Phase 4.4 switched-path payload.
 */
bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi);

/**
 * Phase 3.5 纯算键：跑到声明键为止，**不发射任何 GLSL 文本**。
 *
 * 热路径早退用：调用方拿到 program key 后可直接查内存缓存，命中即完全跳过
 * Phase 4（数千行 GLSL 拼接）与整个 ShaderBuildContext 分配。
 *
 * 返回 false = 本次无法纯算出键（声明闭包读不到磁盘 / 声明输入不完整）——
 * 调用方**必须**退回 CreateMaterialFromDefinition 全量路径，绝不能自己造键。
 * 成功时 out_program_key 与全量路径 GetProgramLink().BuildKey() 逐位相同
 * （回归门 cache.pre-emit-program-key-parity 全变体断言）。
 *
 * 注意：`request.render_template_request` 必须**已经**被解析好（先由
 * SelectSceneRenderTemplateRequest 选定路由与 ambient provider，再由
 * AppendMaterialRenderTemplateRoots 补入 material provider roots）——
 * 两条路径必须吃同一份 request，否则声明闭包不同 ⇒ 键必然分叉。
 */
bool ComputeMaterialProgramKey(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialDefinitionBuildRequest &request,
    const MaterialDefinition &definition,
    ShaderProgramKey &out_program_key);

/**
 * 生成 sampler 预设宏字符串（统一注册机制）。
 *
 * 遍历 sampler_names，为每个名字生成 "#define <name>Sampler <idx>u"。
 * idx 经 SamplerPresetLibrary::GetIndex 查询（查不到则报错并跳过——不生成宏，
 * 让 shader 编译因未定义宏显式失败，而非静默错位成错误采样器），
 * 与运行时 bindless 采样器数组的下标保持一致。空列表返回空字符串。
 */
std::string BuildSamplerMacros(const std::vector<std::string> &sampler_names);

/**
 * 编译 Compositor 模板产出的完整 GLSL → ShaderBuildContext*。
 *
 * 使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param input     canonical compiler input
 * @param mesh_source_document      mesh 模板的结构化 source document
 * @param fragment_source_document  fragment 模板的结构化 source document
 * @param config    编译期配置视图
 * @return          编译好的 ShaderBuildContext*; 失败返回 nullptr
 */
ShaderBuildContext *BuildMaterialStageDocuments(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const ShaderDocument &mesh_source_document,
    const ShaderDocument &fragment_source_document,
    const MaterialCompileConfig &config);

// Diagnostic-only overload. Keeping capture outside MaterialCompileConfig
// preserves the layout of the production compiler configuration.
ShaderBuildContext *BuildMaterialStageDocuments(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const ShaderDocument &mesh_source_document,
    const ShaderDocument &fragment_source_document,
    const MaterialCompileConfig &config,
    MaterialShaderDocumentCapture *document_capture);

}//namespace hgl::graph::mtl
