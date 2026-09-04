#pragma once

/// MaterialShaderEmitter.h — GLSL 发射层（内部头，仅 ShaderGen 内部使用）
///
/// S2-T2.1：把「文本发射」从 MaterialShaderCompiler.cpp 中分离。
///
/// 分工约定（S2 的核心不变量）：
///   - **求解层**（MaterialShaderCompiler.cpp）：做决策——契约、描述符注册、槽位合并，
///     产生 ShaderBuildContext 与 DescriptorSetLayoutAllocator 状态。
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
    ShaderDocument BuildCodeModuleDocument(
        const ShaderCodeResourceManifest *manifest,
        const char *stage = nullptr,
        const char *material = nullptr);

    /// 材质实例 SSBO 的 struct + buffer 声明与 MTL_DATA 别名宏。
    /// material_private_data 为 UserDefined 时返回 true 且不生成。
    /// 失败返回 false 并写 out_error（发射层不依赖求解层的 CompileContext）。
    bool BuildMaterialSSBODeclarations(
        const DescriptorSetLayoutAllocator &descriptor_info,
        SSBOType material_private_data,
        std::string &out_decls,
        std::string &out_macros,
        std::string &out_error);

    bool BuildMaterialResourceDocument(
        const DescriptorSetLayoutAllocator &descriptor_info,
        SSBOType material_private_data,
        ShaderDocument &out_document,
        std::string &out_error);

    /// MaterialDefinition.compile_defines → "#define <name> 1"
    bool BuildCompileDefineDocument(
        const CompositorMaterialBuildConfig &config,
        ShaderDocument &out_document);
    std::string BuildCompileDefineMacros(const CompositorMaterialBuildConfig &config);

    /// mesh 阶段行表声明（l2w_index / mtl_private_data_index + Resolve 函数）
    std::string BuildMeshIndexTableDecls(const DescriptorSetLayoutAllocator &descriptor_info);

    /// FS 阶段行表声明（TextureLayerRowsData named-slot struct + buffer）
    std::string BuildFSIndexTableDecls(const DescriptorSetLayoutAllocator &descriptor_info);

    /// 将模板 source document 与已解出的材质片段合并为最终 stage document。
    /// source document 必须以 Version block 开始。
    bool BuildMaterialStageDocument(
        const ShaderDocument &source_document,
        ShaderStage stage,
        const char *material,
        const CompositorMaterialBuildConfig &config,
        const DescriptorSetLayoutAllocator &descriptor_info,
        SSBOType material_private_data,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics);

}//namespace hgl::graph::mtl
