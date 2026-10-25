#pragma once

namespace hgl::graph::mtl {}

/// MaterialShaderCompiler.h — canonical material input → ShaderBuildContext
///
/// 使用 BuildMaterialStageDocuments 编译阶段源：两个阶段都**物理固定**，
/// `MaterialCompileConfig::fixed_mesh_source` 与 `fixed_fragment_source`
/// 是硬前置（网格阶段=ShaderLibrary/mesh/fixed/<key>.mesh，片段阶段=领域主
/// 着色器），它们本身就是最终编译单元。
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
#include <string>
#include <vector>

namespace hgl::graph
{
    struct ShaderBufferSource;
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl{
/// 构建期观测快照（仅回归门禁用）。
///
/// 文档层删除后它只剩两个标量：本次构建落在哪个网格形状键、该键是否由固定
/// 源满足。想断言阶段文本请看 `ShaderCreateInfo::GetFinalGLSL()`；想断言宏/
/// 环境光看 `EmitDomainOptions*`。快照由 GenericMaterialBuilder 写入，
/// 编译器本身不再触碰它。
struct MaterialShaderBuildCapture
{
    uint64 mesh_source_shape_key = 0;
    bool mesh_source_was_fixed = false;

    void Clear() noexcept
    {
        mesh_source_shape_key = 0;
        mesh_source_was_fixed = false;
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
    // 片段阶段的最终 GLSL 就是这份文本（领域固定主着色器）。硬前置。
    // 见 `MaterialDefinitionBuildRequest::fixed_fragment_source`。
    const char *fixed_fragment_source = nullptr;
    // 完整 mesh stage GLSL 直通（ShaderLibrary/mesh/fixed/<shape_key>.mesh）。硬前置。
    const char *fixed_mesh_source = nullptr;
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
 * 判定材质定义是否需要 payload 行（MTL_ROW）。
 */
bool MaterialDefinitionRequiresPayloadRow(
    GlobalSSBOType material_private_data,
    const MaterialDefinition *material_definition,
    bool &out_required) noexcept;

/**
 * 编译两份物理固定的阶段源 → ShaderBuildContext*。
 *
 * 不再有"模板文档合并 → 序列化"环节：`config.fixed_mesh_source` 与
 * `config.fixed_fragment_source` 缺任一份即硬失败（静默降级会让网格阶段变成
 * 空壳）。使用 SetFinalGLSL() + CreateShaderDirect() 直接编译。
 *
 * @param profile   设备能力 profile
 * @param input     canonical compiler input
 * @param config    编译期配置视图（含两份固定源）
 * @return          编译好的 ShaderBuildContext*; 失败返回 nullptr
 */
ShaderBuildContext *BuildMaterialStageDocuments(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialShaderCompilerInput &input,
    const MaterialCompileConfig &config);

}//namespace hgl::graph::mtl
