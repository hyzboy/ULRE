// 运行时 → Shader 生成器接缝的槽位定义。
//
// 放在数据面（而不是契约层或 ShaderGen 里）是刻意的：
//   · 接口吃的是材质定义 / 构建请求 —— 这些类型本身就在本层，契约层不该反向依赖它们；
//   · 运行时侧（SceneGraph）只读它 ⇒ 不需要链接生成器；
//   · 生成器侧（ShaderGen）只写它 ⇒ 已经在 PUBLIC 链接本层；
//   · 发行形态的 Manifest 后端同样只写它，也在本层内。
// 三方都不必互相链接，契约层则保持零运行时依赖。

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
