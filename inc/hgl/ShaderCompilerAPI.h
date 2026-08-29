#pragma once

// ShaderCompilerAPI.h — 引擎公开的 GLSL 编译接口（封装 GLSLCompiler 插件 D:\GLSLCompiler.dll）
//
// 供示例/工具直接编译 GLSL 到 SPIR-V（绕过 ShaderGen 材质系统）。
// 实现位于 src/ShaderGen/GLSLCompiler.cpp（ULRE.ShaderGen 库）。
// 编译目标已硬性强制 Vulkan 1.4 + SPIR-V 1.6（插件侧 2026-08 决策）。

namespace hgl::graph
{
    struct SPVData
    {
        bool result;
        char *log;
        char *debug_log;

        uint32 *spv_data;
        uint32 spv_length;
    };

    bool        InitShaderCompiler();       ///< 加载并初始化 GLSLCompiler 插件（幂等）
    void        CloseShaderCompiler();

    SPVData *   CompileShader   (const uint32 stage_bit,const char *source);
    void        FreeSPVData     (SPVData *spv_data);
}//namespace hgl::graph
