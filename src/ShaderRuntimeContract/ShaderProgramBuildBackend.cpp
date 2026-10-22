// 运行时 → Shader 生成器接缝的槽位定义。
//
// 放在契约库里（而不是 SceneGraph 或 ShaderGen 里）是刻意的：
//   · 运行时侧（SceneGraph）只读它 ⇒ 不需要链接生成器；
//   · 生成器侧（ShaderGen）只写它 ⇒ 已经在 PUBLIC 链接本库；
//   · 将来 Manifest 后端（发行形态）同样只写它。
// 三方都不必互相链接。

#include <hgl/mtl/ShaderProgramBuildBackend.h>

namespace hgl::graph::mtl
{
    namespace
    {
        ShaderProgramBuildBackend *g_shader_program_build_backend = nullptr;
    }

    void SetShaderProgramBuildBackend(ShaderProgramBuildBackend *backend)
    {
        g_shader_program_build_backend = backend;
    }

    ShaderProgramBuildBackend *GetShaderProgramBuildBackend()
    {
        return g_shader_program_build_backend;
    }
}//namespace hgl::graph::mtl
