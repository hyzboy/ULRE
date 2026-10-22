#ifndef HGL_GLSL_COMPILER_INCLUDE
#define HGL_GLSL_COMPILER_INCLUDE

#include<hgl/type/DataType.h>
#include<hgl/mtl/ShaderCompilerProfileAPI.h>
#include<hgl/mtl/ShaderCompilerSPVData.h>
namespace hgl
{
    namespace graph
    {
        bool        InitShaderCompiler();
        void        CloseShaderCompiler();

        SPVData *   CompileShader   (const uint32 type,const char *source);
        void        FreeSPVData     (SPVData *spv_data);
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
