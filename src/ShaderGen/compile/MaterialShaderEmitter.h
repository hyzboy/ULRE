#pragma once

/// MaterialShaderEmitter.h — GLSL 发射层（内部头，仅 ShaderGen 内部使用）
///
/// S2-T2.1：把「文本发射」从 MaterialShaderCompiler.cpp 中分离。
///
/// 分工约定（S2 的核心不变量）：
///   - **求解层**（MaterialShaderCompiler.cpp）：做决策——契约、描述符注册、槽位合并，
///     产生 ShaderBuildContext 与 DescriptorSetLayoutAllocator 状态。
///   - **发射层**（本文件 + .cpp）：**纯函数，零决策**——只把已解出的状态转成 GLSL 文本。
///     不得读取全局状态、不得改 ctx（AssembleFinalGLSL 例外：终点写入 SetFinalGLSL）。

#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderDocument.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <string>

namespace hgl::graph::mtl
{
    /// 模块代码拼接（manifest 中登记的 ShaderCodeModule 按序拼接）
    std::string BuildCodeModuleGLSL(const ShaderCodeResourceManifest *manifest);
    ShaderDocument BuildCodeModuleDocument(const ShaderCodeResourceManifest *manifest);

    /// 材质实例 SSBO 的 struct + buffer 声明与 MTL_DATA 别名宏。
    /// material_private_data 为 UserDefined 时返回 true 且不生成。
    /// 失败返回 false 并写 out_error（发射层不依赖求解层的 CompileContext）。
    bool BuildMaterialSSBODeclarations(
        const DescriptorSetLayoutAllocator &descriptor_info,
        SSBOType material_private_data,
        std::string &out_decls,
        std::string &out_macros,
        std::string &out_error);

    /// MaterialDefinition.compile_defines → "#define <name> 1"
    std::string BuildCompileDefineMacros(const CompositorMaterialBuildConfig &config);

    /// mesh 阶段行表声明（l2w_index / mtl_private_data_index + Resolve 函数）
    std::string BuildMeshIndexTableDecls(const DescriptorSetLayoutAllocator &descriptor_info);

    /// FS 阶段行表声明（TextureLayerRowsData named-slot struct + buffer）
    std::string BuildFSIndexTableDecls(const DescriptorSetLayoutAllocator &descriptor_info);

    /// 最终组装：把两 stage 的 version 后注入文本（单趟发射器的固化产物——
    /// 历史上支持多点注入，单趟发射后恒为每 stage 恰一段）插入并写入
    /// 各 stage 的 FinalGLSL
    void AssembleFinalGLSL(
        ShaderBuildContext *ctx,
        const std::string &ms_glsl,
        const std::string &fs_glsl,
        const std::string &ms_inject,
        const std::string &fs_inject);
}//namespace hgl::graph::mtl
