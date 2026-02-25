/// S_VertexColor3D_Logic.h — VertexColor3D 材质的纯业务逻辑定义

#pragma once

#include <hgl/graph/mtl/ShaderLogic.h>

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// 业务逻辑代码（纯GLSL函数，无资源声明）
// ─────────────────────────────────────────────────────────────────────────────

constexpr char VERTEX_COLOR_VERTEX_LOGIC[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.Color = vi.Color;
    return vec4(vi.Position, 1.0);
}
)";

/// Fragment Shader 主逻辑
/// 
/// 输入：
///   - 通过全局 Input (Vertex_Output 接口块) 访问 VS 插值数据
/// 
/// 输出：
///   - vec4: 最终颜色
constexpr char VERTEX_COLOR_FRAGMENT_LOGIC[] = R"(
vec4 FragmentShaderBusiness()
{
    return Input.Color;
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// 资源依赖声明
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char* VERTEX_COLOR_VERTEX_RESOURCES[] = {
    "l2w",
    "camera"
};

constexpr const char* const* VERTEX_COLOR_VERTEX_HELPERS = nullptr;

constexpr const char* const* VERTEX_COLOR_FRAGMENT_RESOURCES = nullptr;
constexpr const char* const* VERTEX_COLOR_FRAGMENT_HELPERS = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// 完整材质逻辑定义
// ─────────────────────────────────────────────────────────────────────────────

const VertexShaderLogic VERTEX_COLOR_VERTEX_SHADER_LOGIC = {
    {
        VERTEX_COLOR_VERTEX_LOGIC,
        nullptr,
        VERTEX_COLOR_VERTEX_RESOURCES,
        2,
        VERTEX_COLOR_VERTEX_HELPERS,
        0
    }
};

const FragmentShaderLogic VERTEX_COLOR_FRAGMENT_SHADER_LOGIC = {
    {
        VERTEX_COLOR_FRAGMENT_LOGIC,
        nullptr,
        VERTEX_COLOR_FRAGMENT_RESOURCES,
        0,
        VERTEX_COLOR_FRAGMENT_HELPERS,
        0
    }
};

const MaterialLogicDef VERTEX_COLOR_3D_LOGIC = {
    VERTEX_COLOR_VERTEX_SHADER_LOGIC,
    VERTEX_COLOR_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

} // namespace hgl::graph::mtl
