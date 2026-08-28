#pragma once

#include <cstdint>
#include <cstring>

namespace hgl::graph::mtl
{
    // mesh shader 生成模式（材质 TOML [mesh_shader] mode；缺省 = VertexPassthrough）
    enum class MeshShaderMode : uint8_t
    {
        VertexPassthrough,   // 每线程 1 顶点（默认，模拟 VS）
        LineQuad,            // 每线程 1 线段 → quad
        CharQuad,            // 每线程 1 字符实例 → quad（6 顶点 2 三角形）
    };

    // TOML 值 → 枚举。空串 = VertexPassthrough（缺省语义）；
    // 未知值返回 false（拼写错误 = 显式解析失败，而非静默降级到默认模式）。
    inline bool ParseMeshShaderMode(const char *name, MeshShaderMode &out) noexcept
    {
        if (!name || !name[0])
        {
            out = MeshShaderMode::VertexPassthrough;
            return true;
        }
        if (std::strcmp(name, "VertexPassthrough") == 0) { out = MeshShaderMode::VertexPassthrough; return true; }
        if (std::strcmp(name, "LineQuad") == 0)          { out = MeshShaderMode::LineQuad;          return true; }
        if (std::strcmp(name, "CharQuad") == 0)          { out = MeshShaderMode::CharQuad;          return true; }
        return false;
    }
}
