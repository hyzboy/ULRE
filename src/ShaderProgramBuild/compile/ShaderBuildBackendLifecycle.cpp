// 编译器插件 ↔ 构建后端的生命周期接线。
//
// 编译器桥（ULRE.ShaderCompiler）不认识任何后端；`SetShaderCompilerLifecycleHooks`
// 是唯一的连接方式。注册对象是 TU 级静态量，构造发生在 main 之前，因此
// `InitShaderCompiler()` 被调用时钩子必然已经就位——**前提是这个 TU 真的被链接进
// 可执行文件**。它没有任何对外符号，静态库链接器会把整个 TU 丢掉，所以聚合入口
// `ULRE::Engine` 对 `ULRE.ShaderProgramBuild` 使用 `$<LINK_LIBRARY:WHOLE_ARCHIVE,...>`
// （见根 CMakeLists.txt 的 `ULRE_PROGRAM_BUILD`）。发行形态整个 TU 不存在，钩子保持
// 为空，编译器照常工作而不装任何后端。
//
// 这个 TU 是**存活**的接线，不寄居在任何具体后端里。阶段 5.2 之前它曾住在
// 生成器 TU 里，删生成器时就必须顺带搬家；搬晚一步的症状是「发行形态没有后端」，
// 只在运行期才暴露。现在后端只有领域后端一个，选择器也随之消失。

#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/ShaderCompilerAPI.h>

#include "ShaderProgramBuildBackendInstall.h"

namespace hgl::graph::mtl
{
    namespace
    {
        /// 生命周期钩子的两端。装/卸必须对称：`SetShaderProgramBuildBackend(nullptr)`
        /// 与释放器各自成对，漏一边会让槽位清空却留着释放器（或反之）。
        void InstallShaderProgramBuildBackend()
        {
            InstallDomainShaderBuildBackend();
        }

        void UninstallShaderProgramBuildBackend()
        {
            UninstallDomainShaderBuildBackend();
        }

        struct ShaderProgramBuildLifecycleRegistration
        {
            ShaderProgramBuildLifecycleRegistration()
            {
                hgl::graph::SetShaderCompilerLifecycleHooks(
                    &InstallShaderProgramBuildBackend,
                    &UninstallShaderProgramBuildBackend);
            }
        };//struct ShaderProgramBuildLifecycleRegistration

        ShaderProgramBuildLifecycleRegistration g_shader_program_build_lifecycle_registration;
    }//namespace
}//namespace hgl::graph::mtl
