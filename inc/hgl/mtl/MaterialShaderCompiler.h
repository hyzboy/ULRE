#pragma once

namespace hgl::graph::mtl {}

/// MaterialShaderCompiler.h — canonical material input → ShaderBuildContext
///
/// 使用 BuildMaterialStageDocuments 编译阶段源：片段阶段在
/// `MaterialCompileConfig::fixed_fragment_source` 非空时直通（领域主着色器就是
/// 最终编译单元），网格阶段仍由 `BuildMaterialStageDocument` 拼出文档。
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
    // 片段侧已物理固定：生产路径只在**没有** fixed_fragment_source 时才构建片段
    // 文档，而该字段现在是硬前置，所以这里恒为空。想断言片段文本请看
    // `ShaderCreateInfo::GetFinalGLSL()`，断言宏/环境光看 `EmitDomainOptions*`。
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
    // 非空 ⇒ 片段阶段的最终 GLSL 就是这份文本（领域固定主着色器）：跳过
    // 片段文档合并与序列化，其余（网格阶段、link、元数据）流程不变。
    // 见 `MaterialDefinitionBuildRequest::fixed_fragment_source`。
    const char *fixed_fragment_source = nullptr;
};

class ShaderBuildContext;

bool FinalizeShaderBuildContext(
    ShaderBuildContext *build_spec);

// ── 生成期入口（构建上下文 / 声明键 / 顶点 ABI）──────────────────────────────
// 这些函数要么发射网格阶段 GLSL，要么读 ShaderLibrary/*.glsl 的声明闭包。
// 它们原先声明在 MaterialDefinitionRegistry.h（数据面）里，会把「发行形态用不到的
// 生成期能力」挂在数据面的头文件上；现在收到本头，数据面只留纯数据能力。
// 实现位置：GenericMaterialBuilder.cpp / VertexABIBuilder.cpp。

/**
 * Build the resolver-derived vertex ABI without compiling shaders or mutating
 * a program/cache. This is the explicit Phase 4.4 switched-path payload.
 */
bool BuildResolvedMaterialVertexABI(
    const MaterialDefinition &definition,
    const MaterialDefinitionBuildRequest &request,
    MaterialResolvedVertexABI &out_abi);

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
 * 诊断用：把最终 GLSL 文本落盘，供人工比对。
 *
 * 仅在开关 `ULRE_DEBUG.toml` 的 `channels.shadergen = true` 开启时生效，
 * 其余情况为空操作。输出目录 = <ShaderCacheRoot>/glsldump。
 */
void DumpShaderGenGLSL(const char *stage, const std::string &text);

/**
 * 把模板 source document 与已解出的材质资源声明合并为最终 stage document。
 * source document 必须以 Version block 开始。
 *
 * 无生成器终态下只剩 map-shader（网格）阶段使用；片段阶段由领域固定主
 * 着色器文件直接承载，不再经本函数。
 */
bool BuildMaterialStageDocument(
    const ShaderDocument &source_document,
    ShaderStage stage,
    const char *material,
    const MaterialCompileConfig &config,
    GlobalSSBOType material_private_data,
    ShaderDocument &out_document,
    ShaderDocumentDiagnostics &out_diagnostics);

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
