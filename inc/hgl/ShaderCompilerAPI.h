// ShaderCompilerAPI.h — 引擎公开的 GLSL 编译接口（封装 GLSLCompiler 插件）
//
// 供示例/工具/运行时直接编译 GLSL 到 SPIR-V，或对已有 SPIR-V 做反射。
// 实现位于 src/ShaderCompiler/GLSLCompiler.cpp（ULRE.ShaderCompiler 库）。
// 编译目标已硬性强制 Vulkan 1.4 + SPIR-V 1.6（插件侧 2026-08 决策）。
//
// 插件按裸名 "GLSLCompiler" + 平台插件扩展名查找，先试当前工作目录，再回退到
// 可执行文件目录；不依赖任何固定盘符路径。
//
// **本模块不认识任何「构建后端」**：材质程序构建侧（ULRE.ShaderProgramBuild）或直通后端要装进
// 运行时接缝（inc/hgl/mtl/ShaderProgramBuildBackend.h）时，通过
// SetShaderCompilerLifecycleHooks() 注册一对钩子，由插件就绪 / 关闭时回调。
// 这样编译器模块对上层零反向依赖，发行形态（ULRE_ENABLE_SHADERGEN=OFF）
// 也能照常把它编进运行时。

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

    // ── 构建后端生命周期钩子 ────────────────────────────────────────────────────
    // 由「往接缝里装后端」的那一层注册（生成器侧 TU 的静态注册对象），编译器模块
    // 只在插件就绪后 / 关闭前原样回调，绝不反向依赖任何实现后端的库。
    // 可传 nullptr（表示该时机无动作）。
    typedef void (*ShaderCompilerLifecycleHook)();

    void        SetShaderCompilerLifecycleHooks(ShaderCompilerLifecycleHook on_ready,
                                               ShaderCompilerLifecycleHook on_close);
}//namespace hgl::graph
