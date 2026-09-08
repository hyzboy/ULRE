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

    /// 将模板 source document 与已解出的材质片段合并为最终 stage document。
    /// source document 必须以 Version block 开始。
    bool BuildMaterialStageDocument(
        const ShaderDocument &source_document,
        ShaderStage stage,
        const char *material,
        const MaterialCompileConfig &config,
        SSBOType material_private_data,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics);

}//namespace hgl::graph::mtl
