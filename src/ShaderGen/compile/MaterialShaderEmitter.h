#pragma once

/// MaterialShaderEmitter.h — GLSL 发射层（内部头，仅 ShaderGen 内部使用）
///
/// S2-T2.1：把「文本发射」从 MaterialShaderCompiler.cpp 中分离。
///
/// 分工约定（S2 的核心不变量）：
///   - **求解层**（MaterialShaderCompiler.cpp）：做决策——契约、schema 构建、槽位合并，
///     产生 ShaderBuildContext 状态。
///   - **发射层**（本文件 + .cpp）：**纯函数，零决策**——只把已解出的状态转成
///     ShaderDocument 和离线 GLSL 文本。

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderDocument.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <string>

namespace hgl::graph::mtl
{
    bool BuildCodeModuleDocument(
        const ShaderCodeResourceManifest *manifest,
        const char *stage,
        const char *material,
        ShaderDocument &out_document);
    // ── 以下构建函数均为 .cpp 内部实现（2026-09 de-export）：
    //    BuildMaterialSSBODeclarations / BuildMaterialResourceDocument /
    //    BuildCompileDefineDocument / BuildMeshIndexTableDecls /
    //    BuildFSIndexTableDecls —— 仅被 BuildMaterialStageDocument 消费。──

    /// 材质是否会发射 `MTL_ROW`，即**程序真的读 payload 行**。
    ///
    /// 判据 = 材质提供的 GLSL 模块（`material_source_module_name` 与
    /// `surface_module_name` 两个槽位）是否声明 `@ulre require Resource MaterialData`
    /// （模块元数据）。不靠模块名猜、不靠纹理声明有无来猜 —— 无纹理材质
    /// （VertexColor / Checkerboard / 纯 Surface 天空）同样不读 payload 行。
    /// 仅当材质没有定义、或模块名存在但注册表查不到时才返回 false
    /// （= 查询失败，调用方必须报错）。
    ///
    /// 调用者：MaterialShaderCompiler 把它折成
    /// `ShaderBuildContext::RequiresRecipePayloadRow()`，让运行期能区分
    /// 「payload 行需求」与「纯纹理引用行需求」（见 ShaderBuildContext 同名字段注释）。
    bool MaterialDefinitionRequiresPayloadRow(
        GlobalSSBOType material_private_data,
        const MaterialDefinition *material_definition,
        bool &out_required) noexcept;

    /// 诊断用：把最终 GLSL 文本落盘，供人工比对（原本散落在两处且路径硬编码）。
    ///
    /// 仅在开关 `ULRE_DEBUG=shadergen`（旧变量 ULRE_DUMP_GLSL）开启时生效，
    /// 其余情况为空操作。开关是运行时的，Debug/Release 均有效。
    /// 输出目录 = <ShaderCacheRoot>/glsldump；ShaderCacheRoot 由
    /// ULRE_SHADER_CACHE_PATH > exe 目录 > cwd 解析（见 ShaderCacheRoot.h），
    /// 因此不绑定任何机器绝对路径。文件名带自增序号，避免不同材质互相覆盖。
    ///
    /// 生产路径的 GLSL 归档由 ShaderArtifactStore::SaveStageGLSL 负责，
    /// 与本函数无关（本函数只是调试便利）。
    void DumpShaderGenGLSL(const char *stage, const std::string &text);

    /// 将模板 source document 与已解出的材质片段合并为最终 stage document。
    /// source document 必须以 Version block 开始。
    bool BuildMaterialStageDocument(
        const ShaderDocument &source_document,
        ShaderStage stage,
        const char *material,
        const MaterialCompileConfig &config,
        GlobalSSBOType material_private_data,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics);

}//namespace hgl::graph::mtl
