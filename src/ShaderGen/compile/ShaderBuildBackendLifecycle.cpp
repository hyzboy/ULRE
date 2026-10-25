// 编译器插件 ↔ 构建后端的生命周期接线。
//
// 编译器桥（ULRE.ShaderCompiler）不认识任何后端；`SetShaderCompilerLifecycleHooks`
// 是唯一的连接方式。注册对象是 TU 级静态量，构造发生在 main 之前，因此
// `InitShaderCompiler()` 被调用时钩子必然已经就位——**前提是这个 TU 真的被链接进
// 可执行文件**。它没有任何对外符号，静态库链接器会把整个 TU 丢掉，所以聚合入口
// `ULRE::Engine` 对 `ULRE.ShaderGen` 使用 `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`
// （见根 CMakeLists.txt 的 `ULRE_SHADERGEN`）。发行形态整个 TU 不存在，钩子保持
// 为空，编译器照常工作而不装任何后端。
//
// 这个 TU 独立于 `ShaderGenProgramBuildBackend.cpp`（阶段 5.2 要删掉那个文件）：
// 接线是**存活**的东西，不该寄居在待删的生成器 TU 里。寄居的后果是删生成器时
// 必须同时把接线搬走，而搬走那一刻的失败只会表现成「发行形态没有后端」这种
// 事后才发现的症状。

#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/ShaderCompilerAPI.h>

#include "ShaderGenBackendInstall.h"

namespace hgl::graph::mtl
{
    namespace
    {
        /// 生命周期钩子两端的**选择器**：安装时读一次环境变量并记住结论，
        /// 卸载必须装/卸对称（`SetShaderProgramBuildBackend(nullptr)` 对错的
        /// 后端调用会把槽位清空但漏掉释放器，或反之）。
        bool g_use_shadergen_build_backend = false;

        void InstallSelectedShaderProgramBuildBackend()
        {
            g_use_shadergen_build_backend = IsShaderGenBuildBackendRequested();

            if (g_use_shadergen_build_backend)
                InstallShaderGenProgramBuildBackend();
            else
                InstallDomainShaderBuildBackend();
        }

        void UninstallSelectedShaderProgramBuildBackend()
        {
            if (g_use_shadergen_build_backend)
                UninstallShaderGenProgramBuildBackend();
            else
                UninstallDomainShaderBuildBackend();

            g_use_shadergen_build_backend = false;
        }

        struct ShaderGenBackendLifecycleRegistration
        {
            ShaderGenBackendLifecycleRegistration()
            {
                hgl::graph::SetShaderCompilerLifecycleHooks(
                    &InstallSelectedShaderProgramBuildBackend,
                    &UninstallSelectedShaderProgramBuildBackend);
            }
        };//struct ShaderGenBackendLifecycleRegistration

        ShaderGenBackendLifecycleRegistration g_shadergen_backend_lifecycle_registration;
    }//namespace
}//namespace hgl::graph::mtl
