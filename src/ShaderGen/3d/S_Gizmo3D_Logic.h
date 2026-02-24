/// S_Gizmo3D_Logic.h — Gizmo3D 材质的纯业务逻辑定义
/// 
/// 特点：内置简化的 Blinn-Phong 光照
///   - 用于 3D Gizmo 控件（移动/旋转/缩放手柄）
///   - 内置太阳光方向和参数（无需外部传入）
///   - 需要法线进行光照计算
///   - 需要 MaterialInstance 获取颜色

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
///   - Normal: vec3 (来自 VBO - 法线)
///   - mi: MaterialInstance (框架提取，包含颜色)
/// 
/// 输出：
///   - Output.Normal: 变换后的法线（用于光照）
///   - Output.Position: 世界坐标位置（用于视角相关计算）
///   - Output.MaterialInstanceID: 传递材质实例索引（用于 FS）
///   - 返回值: 本地坐标
constexpr char GIZMO_3D_VERTEX_LOGIC[] = R"(
vec4 VertexMain(vec3 Position, vec3 Normal, MaterialInstance mi) 
{
    // 传递变换后的法线
    Output.Normal = TransformNormal(Normal);
    
    // 传递世界坐标（用于视角计算）
    Output.Position = GetWorldPos(Position);
    
    // 传递材质实例索引到 Fragment Shader
    Output.MaterialInstanceID = MaterialInstanceID;
    
    // 返回本地坐标
    return vec4(Position, 1.0);
}
)";

/// Fragment Shader 主逻辑
/// 
/// 输入：
///   - Input.Normal: 插值后的法线
///   - Input.Position: 插值后的世界坐标
///   - mi: MaterialInstance (框架提取，包含颜色)
/// 
/// 输出：
///   - FragColor: 最终颜色（包含光照）
/// 
/// 说明：使用简化的 Blinn-Phong 光照模型
constexpr char GIZMO_3D_FRAGMENT_LOGIC[] = R"(
vec4 FragmentMain(vec3 Normal, vec4 Position, MaterialInstance mi) 
{
    // 内置参数（专为 Gizmo 优化）
    const vec3 SUN_DIRECTION = vec3(0.655386, 0.491539, 0.573462);  // normalized(8,6,7)
    const vec3 SUN_COLOR = vec3(1.0, 1.0, 1.0);
    const float SPECULAR_POWER = 16.0;
    
    // 漫反射强度（Lambert + 环境光）
    float diffuse_intensity = 0.5 * max(dot(Normal, SUN_DIRECTION), 0.0) + 0.5;
    
    // 漫反射颜色
    vec3 diffuse_color = diffuse_intensity * SUN_COLOR * mi.Color.rgb;
    
    // 高光计算
    vec3 specular_color = vec3(0);
    if (diffuse_intensity > 0.5)  // 只在受光面计算高光
    {
        // Blinn-Phong 半角向量
        vec3 view_dir = normalize(GetCameraPos() - Position.xyz);
        vec3 half_vector = normalize(SUN_DIRECTION + view_dir);
        
        float specular = max(dot(half_vector, Normal), 0.0);
        specular_color = pow(specular, SPECULAR_POWER) * SUN_COLOR;
    }
    
    // 合并漫反射和高光
    return vec4(diffuse_color + specular_color, 1.0);
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// 资源依赖声明
// ─────────────────────────────────────────────────────────────────────────────

/// Vertex Shader 需要的资源
constexpr const char* GIZMO_3D_VERTEX_RESOURCES[] = {
    "MaterialInstanceData",  // 材质实例（颜色）
    "LocalToWorld"           // 变换矩阵
};

/// Vertex Shader 需要的高级辅助函数
constexpr const char* GIZMO_3D_VERTEX_HELPERS[] = {
    BuiltinHelpers::TransformNormal,  // 法线变换
    "GetWorldPos"                     // 世界坐标计算（可能需要添加到 BuiltinHelpers）
};

/// Fragment Shader 需要的资源
constexpr const char* GIZMO_3D_FRAGMENT_RESOURCES[] = {
    "CameraInfo"  // 相机位置（用于视角计算）
};

/// Fragment Shader 需要的高级辅助函数
constexpr const char* GIZMO_3D_FRAGMENT_HELPERS[] = {
    "GetCameraPos"  // 获取相机位置（可能需要添加到 BuiltinHelpers）
};

// ─────────────────────────────────────────────────────────────────────────────
// 完整材质逻辑定义
// ─────────────────────────────────────────────────────────────────────────────

/// Gizmo3D 材质的纯逻辑定义
/// 
/// 特点：
///   - 内置简化的 Blinn-Phong 光照
///   - 专为 3D Gizmo 控件优化
///   - 展示了自定义光照计算的用法
constexpr MaterialLogicDef GIZMO_3D_LOGIC = {
    // Vertex Shader 逻辑
    .vertex = {
        .main_logic = GIZMO_3D_VERTEX_LOGIC,
        .custom_functions = nullptr,
        .required_resources = GIZMO_3D_VERTEX_RESOURCES,
        .required_resource_count = 2,
        .required_helpers = GIZMO_3D_VERTEX_HELPERS,
        .required_helper_count = 2
    },
    
    // Fragment Shader 逻辑
    .fragment = {
        .main_logic = GIZMO_3D_FRAGMENT_LOGIC,
        .custom_functions = nullptr,
        .required_resources = GIZMO_3D_FRAGMENT_RESOURCES,
        .required_resource_count = 1,
        .required_helpers = GIZMO_3D_FRAGMENT_HELPERS,
        .required_helper_count = 1
    },
    
    // 其他 Shader 阶段（未使用）
    .geometry = nullptr,
    .tess_control = nullptr,
    .tess_eval = nullptr
};

// ─────────────────────────────────────────────────────────────────────────────
// 设计说明
// ─────────────────────────────────────────────────────────────────────────────

/*
### 旧方式 (M_Gizmo3D.cpp)

VS:
```cpp
void main()
{
    HandoverMI();  // 传递材质实例索引
    Output.Normal   = GetNormal();
    Output.Position = GetPosition3D();
    gl_Position     = Output.Position;
}
```

FS:
```cpp
void main()
{
    MaterialInstance mi = GetMI();  // 手动获取材质实例
    
    float intensity = 0.5 * max(dot(Input.Normal, SUN_DIRECTION), 0.0) + 0.5;
    vec3 direct_color = intensity * SUN_COLOR * mi.Color.rgb;
    
    // ... 高光计算
    
    FragColor = vec4(direct_color + spec_color, 1.0);
}
```

问题：
❌ 业务代码混杂框架调用（HandoverMI, GetMI, GetNormal, GetPosition3D）
❌ 在 main() 中直接写业务逻辑
❌ 依赖隐式（需要读代码才知道要 Normal 和相机位置）


### 新方式 (S_Gizmo3D_Logic.h - 本文件)

VS:
```cpp
vec4 VertexMain(vec3 Position, vec3 Normal, MaterialInstance mi) {
    Output.Normal = TransformNormal(Normal);
    Output.Position = GetWorldPos(Position);
    Output.MaterialInstanceID = MaterialInstanceID;
    return vec4(Position, 1.0);
}
```

FS:
```cpp
vec4 FragmentMain(vec3 Normal, vec4 Position, MaterialInstance mi) {
    // 纯光照计算，无框架调用
    ...
    return vec4(diffuse_color + specular_color, 1.0);
}
```

优势：
✅ 业务代码只有光照算法（纯计算）
✅ 依赖显式（参数即文档：Normal, Position, MaterialInstance）
✅ 框架负责：
   - 准备数据（提取 mi, 计算世界坐标等）
   - 生成 main() 函数
   - 应用变换


### 辅助函数需求

这个材质引入了新的辅助函数需求：
- `TransformNormal(vec3)`: 法线变换（考虑非均匀缩放）
- `GetWorldPos(vec3)`: 计算世界坐标
- `GetCameraPos()`: 获取相机位置

这些将在 Stage 3（框架辅助函数库）中实现。
*/

} // namespace hgl::graph::mtl
