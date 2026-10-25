#ifndef HGL_GLSL_COMPILER_INCLUDE
#define HGL_GLSL_COMPILER_INCLUDE

#include<hgl/type/DataType.h>
#include<hgl/type/String.h>
#include<hgl/type/ValueArray.h>
#include<hgl/mtl/ShaderCompilerProfileAPI.h>
#include<hgl/mtl/ShaderCompilerSPVData.h>

struct ShaderFlatView;

namespace hgl
{
    namespace graph
    {
        bool        InitShaderCompiler();
        void        CloseShaderCompiler();

        SPVData *   CompileShader   (const uint32 type,const char *source);

        // 带诊断的编译入口。失败时把 glslang（或初始化/BOM）的原始报错写进
        // `*out_diagnostic`（可为 nullptr）。没有它，调用方只能看到
        // `CompileShader` 返回 nullptr —— SPVData 已被释放，日志无从取回。
        SPVData *   CompileShader   (const uint32 type,const char *source,
                                     AnsiString *out_diagnostic);
        void        FreeSPVData     (SPVData *spv_data);

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
    }//namespace graph

    namespace graph::mtl
    {
        // 把「生成器」装成运行时认的 ShaderProgramBuildBackend（见
        // inc/hgl/mtl/ShaderProgramBuildBackend.h）。由 InitShaderCompiler/CloseShaderCompiler
        // 负责成对调用；单独暴露是为了回归门能只测接缝而不启动 glslang。
        void        InstallShaderGenProgramBuildBackend();
        void        UninstallShaderGenProgramBuildBackend();

        // ShaderCreateInfo 的 SPV 释放回调安装（生成器侧 TU 定义）
        void        InstallShaderCreateInfoSPVReleaser();
        void        UninstallShaderCreateInfoSPVReleaser();
    }//namespace graph::mtl
}//namespace hgl
#endif//HGL_GLSL_COMPILER_INCLUDE
