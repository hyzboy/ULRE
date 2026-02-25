/// S_VertexColor3D_Logic.h — VertexColor3D 材质的纯业务逻辑定义
/// 
/// 特点：最简单的材质，只传递顶点颜色
///   - 不需要 MaterialInstance（顶点颜色直接来自 VBO）
///   - 不需要任何高级辅助函数
///   - VS 直接传递，FS 直接输出

#pragma once

#include <hgl/graph/mtl/ShaderLogic.h>

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// 业务逻辑代码（纯GLSL函数，无资源声明）
// ─────────────────────────────────────────────────────────────────────────────

/// Vertex Shader 主逻辑
/// 
/// 输入参数：
///   - Position: vec3 (来自 VBO)
///   - Color: vec4 (来自 VBO - 顶点颜色)
/// 
/// 输出：
///   - Output.Color: 传递给 Fragment Shader
///   - 返回值: 本地坐标（框架会自动应用变换）
/// 
/// 说明：这是最简单的材质，只做数据传递，无任何计算
constexpr char VERTEX_COLOR_VERTEX_LOGIC[] = R"(
vec4 VertexMain(vec3 Position, vec4 Color) 
{
    // 直接传递顶点颜色到下一阶段
    Output.Color = Color;
    
    // 返回本地坐标
    return vec4(Position, 1.0);
}
)";

/// Fragment Shader 主逻辑
/// 
/// 输入：
///   - Input.Color: 来自 Vertex Shader 的插值结果
/// 
/// 输出：
///   - FragColor: 最终颜色
constexpr char VERTEX_COLOR_FRAGMENT_LOGIC[] = R"(
vec4 FragmentMain() 
{
    // 直接输出插值后的颜色
    return Input.Color;
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// 资源依赖声明
// ─────────────────────────────────────────────────────────────────────────────

/// Vertex Shader 需要的资源
/// VertexColor3D 只需要变换矩阵，不需要材质实例
constexpr const char* VERTEX_COLOR_VERTEX_RESOURCES[] = {
    "LocalToWorld"  // 用于坐标变换
};

/// 不需要任何高级辅助函数
constexpr const char** VERTEX_COLOR_VERTEX_HELPERS = nullptr;

/// Fragment Shader 不需要额外资源
constexpr const char** VERTEX_COLOR_FRAGMENT_RESOURCES = nullptr;
constexpr const char** VERTEX_COLOR_FRAGMENT_HELPERS = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// 完整材质逻辑定义
// ─────────────────────────────────────────────────────────────────────────────

/// VertexColor3D 材质的纯逻辑定义
/// 
/// 特点：
///   - 最简单的材质（只传递数据，无计算）
///   - 不需要 MaterialInstance
///   - 演示了纯数据传递的场景
constexpr MaterialLogicDef VERTEX_COLOR_3D_LOGIC = {
    // Vertex Shader 逻辑
    .vertex = {
        .main_logic = VERTEX_COLOR_VERTEX_LOGIC,
        .custom_functions = nullptr,
        .required_resources = VERTEX_COLOR_VERTEX_RESOURCES,
        .required_resource_count = 1,  // 只要 LocalToWorld
        .required_helpers = VERTEX_COLOR_VERTEX_HELPERS,
        .required_helper_count = 0
    },
    
    // Fragment Shader 逻辑
    .fragment = {
        .main_logic = VERTEX_COLOR_FRAGMENT_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    
    // 其他 Shader 阶段（未使用）
    .geometry = nullptr,
    .tess_control = nullptr,
    .tess_eval = nullptr
};

// ─────────────────────────────────────────────────────────────────────────────
// 对比说明
// ─────────────────────────────────────────────────────────────────────────────

/*
### 旧方式 (M_VertexColor3D.cpp)

```cpp
constexpr const char vs_main[] = R"(
void main()
{
    Output.Color=Color;  // 直接从输入读取
    gl_Position=GetPosition3D();  // 手动调用变换
}
)";
```

问题：
❌ 直接在 main() 中写逻辑
❌ 混杂了业务逻辑和框架调用（GetPosition3D）


### 新方式 (S_VertexColor3D_Logic.h - 本文件)

```cpp
vec4 VertexMain(vec3 Position, vec4 Color) {
    Output.Color = Color;
    return vec4(Position, 1.0);
}
```

优势：
✅ 纯业务逻辑函数（参数清晰）
✅ 框架负责调用 GetPosition3D 和应用变换
✅ main() 由框架生成：
    void main() {
        vec4 local_pos = VertexMain(Position, Color);
        gl_Position = ViewProj * LocalToWorld * local_pos;
    }


### 与 PureColor3D 的对比

| 特性 | PureColor3D | VertexColor3D |
|------|-------------|---------------|
| **颜色来源** | MaterialInstance | VBO 顶点属性 |
| **参数** | (Position, mi) | (Position, Color) |
| **资源依赖** | MaterialInstanceData + LocalToWorld | LocalToWorld |
| **复杂度** | 需要材质实例 | 最简单 |

这展示了新接口的灵活性：
- 需要材质实例的：传 MaterialInstance 参数
- 只用顶点属性的：直接传对应的顶点属性
- 框架根据参数类型自动准备数据
*/

} // namespace hgl::graph::mtl
