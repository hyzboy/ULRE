// ShaderCompilerAPI.h — 引擎公开的 GLSL 编译接口（封装 GLSLCompiler 插件）
//
// 供示例/工具/运行时直接编译 GLSL 到 SPIR-V，或对已有 SPIR-V 做反射。
// 实现位于 src/ShaderCompiler/GLSLCompiler.cpp（ULRE.ShaderCompiler 库）。
// 编译目标已硬性强制 Vulkan 1.4 + SPIR-V 1.6（插件侧 2026-08 决策）。
//
// 插件按裸名 "GLSLCompiler" + 平台插件扩展名查找，先试当前工作目录，再回退到
// 可执行文件目录；不依赖任何固定盘符路径。
//
// **本模块不认识任何「构建后端」**：材质程序构建侧（ULRE.ShaderProgramBuild）由
// AppFramework（开发形态）或门禁/工具（显式调用）自行装进运行时接缝
// （inc/hgl/mtl/ShaderProgramBuildBackend.h），编译器模块对上层零反向依赖。
// 发行形态（ULRE_ENABLE_SHADER_PROGRAM_BUILD=OFF）没有后端可装，照常编进运行时。

#pragma once

#include <hgl/type/DataType.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <hgl/mtl/ShaderCompilerSPVData.h>      ///< SPVData 的唯一真源（跨模块 ABI 布局）

struct ShaderFlatView;

namespace hgl::graph
{
    bool        InitShaderCompiler();       ///< 加载并初始化 GLSLCompiler 插件（幂等）
    void        CloseShaderCompiler();

    SPVData *   CompileShader   (const uint32 stage_bit,const char *source);
    void        FreeSPVData     (SPVData *spv_data);

    // 带诊断的编译入口。失败时把 glslang（或初始化 / BOM）的原始报错写进
    // `*out_diagnostic`（可为 nullptr）。没有它，调用方只能看到
    // `CompileShader` 返回 nullptr —— SPVData 已被释放，日志无从取回。
    SPVData *   CompileShader   (const uint32 stage_bit,const char *source,
                                 AnsiString *out_diagnostic);

    // Copies plugin-owned reflection output into flat_storage; out_view
    // points into that storage and remains valid until it is modified.
    bool        ReflectSPVData(
        SPVData *spv_data,
        uint32 shader_stage,
        const char *entrypoint,
        ValueArray<uint8> &flat_storage,
        ::ShaderFlatView &out_view);
    bool        ReflectSPVData(
        const uint32 *spv_data,
        size_t spv_size,
        uint32 shader_stage,
        const char *entrypoint,
        ValueArray<uint8> &flat_storage,
        ::ShaderFlatView &out_view);
    bool        GetSPVPushConstantEnd(
        const uint32 *spv_data,
        size_t spv_size,
        uint32 shader_stage,
        uint32 &out_end);
}//namespace hgl::graph
