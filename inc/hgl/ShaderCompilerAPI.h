// ShaderCompilerAPI.h — 引擎公开的 GLSL 编译接口（封装 GLSLCompiler 插件）
//
// 供示例/工具直接编译 GLSL 到 SPIR-V（绕过 ShaderGen 材质系统）。
// 实现位于 src/ShaderGen/compile/GLSLCompiler.cpp（ULRE.ShaderGen 库）。
// 编译目标已硬性强制 Vulkan 1.4 + SPIR-V 1.6（插件侧 2026-08 决策）。
//
// 插件按裸名 "GLSLCompiler" + 平台插件扩展名查找，先试当前工作目录，再回退到
// 可执行文件目录；不依赖任何固定盘符路径。

#pragma once

#include <hgl/type/DataType.h>
#include <hgl/mtl/ShaderCompilerSPVData.h>      ///< SPVData 的唯一真源（跨模块 ABI 布局）

namespace hgl::graph
{
    bool        InitShaderCompiler();       ///< 加载并初始化 GLSLCompiler 插件（幂等）
    void        CloseShaderCompiler();

    SPVData *   CompileShader   (const uint32 stage_bit,const char *source);
    void        FreeSPVData     (SPVData *spv_data);
}//namespace hgl::graph
