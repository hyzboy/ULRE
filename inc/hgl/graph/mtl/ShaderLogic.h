/// ShaderLogic.h — 纯业务逻辑接口（Stage 1 重构产物）
///
/// 核心设计原则：
///   ✅ 业务逻辑代码只包含纯计算（无 layout(...) 声明）
///   ✅ 资源依赖通过名字引用（具体布局由框架统一生成）
///   ✅ 辅助函数（GetMI/GetWorldPos等）由框架提供或显式注入
///
/// 解决的问题：
///   ❌ 旧方式：业务代码混杂 layout 声明 → Generator 又生成 → 重复冲突
///   ✅ 新方式：业务代码纯逻辑 → 资源声明统一生成 → 无重复
///
/// 使用示例：
///   // 业务代码（S_PureColor3D.h）
///   constexpr char PURE_COLOR_VERTEX_LOGIC[] = R"(
///       vec4 ComputeVertexOutput(vec3 Position, uint MaterialInstanceID) {
///           MaterialInstance mi = GetMI();  // GetMI 由框架提供
///           return vec4(Position, 1.0);
///       }
///   )";
///
///   // 材质定义
///   ShaderLogicBlock vertex_logic = {
///       .main_logic = PURE_COLOR_VERTEX_LOGIC,
///       .required_resources = {"MaterialInstanceData", "LocalToWorld"},
///       .required_helpers = {"GetMI", "GetWorldPos"}
///   };

#pragma once

#include <hgl/type/String.h>
#include <hgl/type/List.h>

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// 资源依赖声明（只描述需要什么，不描述如何声明）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 资源依赖项
 * 业务逻辑通过名字引用资源，具体的 layout(...) 声明由 ResourceLayoutGenerator 统一生成
 */
struct ResourceDependency {
    const char* name;           // 资源名称，如 "MaterialInstanceData", "ViewProj"
    const char* resource_type;  // 类型提示：buffer/uniform/sampler/image
};

/**
 * 辅助函数依赖
 * 业务逻辑可能调用框架提供的函数（GetMI/GetWorldPos等）
 */
struct HelperFunctionDependency {
    const char* name;           // 函数名，如 "GetMI", "GetWorldPos"
    const char* category;       // 类别：material/transform/lighting/output
};

// ─────────────────────────────────────────────────────────────────────────────
// 纯业务逻辑块（不包含资源声明）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Shader 业务逻辑块
 * 
 * 核心约定：
 *   - main_logic: 只包含计算代码，不写 layout(...)
 *   - custom_functions: 自定义辅助函数（可选）
 *   - required_resources: 需要哪些资源（只是名字列表）
 *   - required_helpers: 需要哪些框架函数（只是名字列表）
 * 
 * 框架职责：
 *   1. 根据 required_resources 生成所有 layout(...) 声明
 *   2. 根据 required_helpers 注入对应的框架函数代码
 *   3. 将 custom_functions 和 main_logic 插入到合适位置
 *   4. 组装成完整的 GLSL Shader
 */
struct ShaderLogicBlock {
    /// 主业务逻辑（必需）
    /// Vertex Shader 示例: vec4 VertexMain(vec3 position, ...) { ... }
    /// Fragment Shader 示例: vec4 FragmentMain() { ... }
    const char* main_logic;

    /// 自定义辅助函数（可选）
    /// 示例: "vec3 ComputeNormal(vec3 n) { return normalize(n); }"
    const char* custom_functions;

    /// 需要的资源（只是名字数组）
    /// 示例: {"MaterialInstanceData", "LocalToWorld", "ViewProj"}
    const char** required_resources;
    uint32_t required_resource_count;

    /// 需要的框架辅助函数（只是名字数组）
    /// 示例: {"GetMI", "GetWorldPos", "GetNormal"}
    const char** required_helpers;
    uint32_t required_helper_count;
};

/**
 * Vertex Shader 业务逻辑
 */
struct VertexShaderLogic : public ShaderLogicBlock {
    // 预留扩展字段
    // 如：是否需要切线空间、是否输出世界坐标等
};

/**
 * Fragment Shader 业务逻辑
 */
struct FragmentShaderLogic : public ShaderLogicBlock {
    // 预留扩展字段
    // 如：是否需要深度测试、是否有透明度等
};

// ─────────────────────────────────────────────────────────────────────────────
// 完整材质逻辑定义（组合 Vertex + Fragment）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 材质逻辑定义（纯逻辑部分）
 * 
 * 配合 FixedMaterialDef 使用：
 *   - FixedMaterialDef: 定义资源布局（vertex entries, descriptor entries）
 *   - MaterialLogicDef: 定义业务逻辑（纯计算代码）
 * 
 * 两者分离后：
 *   - ResourceLayoutGenerator 从 FixedMaterialDef 生成 layout(...) 声明
 *   - ShaderLogicComposer 从 MaterialLogicDef 组装业务代码
 *   - 最终合成器无缝拼接两者
 */
struct MaterialLogicDef {
    VertexShaderLogic vertex;
    FragmentShaderLogic fragment;
    
    // 可选：Geometry/TessControl/TessEval Shader
    ShaderLogicBlock* geometry;
    ShaderLogicBlock* tess_control;
    ShaderLogicBlock* tess_eval;
};

// ─────────────────────────────────────────────────────────────────────────────
// 辅助函数注册表（框架提供）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 框架内置辅助函数
 * 业务代码可以直接调用，无需自己实现
 */
namespace BuiltinHelpers {
    // 材质实例相关
    constexpr char GetMI[] = "GetMI";                   // MaterialInstance GetMI()
    constexpr char GetMaterialColor[] = "GetMaterialColor"; // vec4 GetMaterialColor()
    
    // 变换相关
    constexpr char GetWorldPos[] = "GetWorldPos";       // vec4 GetWorldPos()
    constexpr char GetClipPos[] = "GetClipPos";         // vec4 GetClipPos()
    constexpr char GetLocalToWorld[] = "GetLocalToWorld"; // mat4 GetLocalToWorld()
    constexpr char GetNormalMatrix[] = "GetNormalMatrix"; // mat3 GetNormalMatrix()
    
    // 光照相关
    constexpr char GetNormal[] = "GetNormal";           // vec3 GetNormal(vec3 local_normal)
    constexpr char ComputeLighting[] = "ComputeLighting"; // vec3 ComputeLighting(...)
    
    // 输出相关
    constexpr char ApplyAlpha[] = "ApplyAlpha";         // vec4 ApplyAlpha(vec3 color, float alpha)
}

// ─────────────────────────────────────────────────────────────────────────────
// 使用示例（文档）
// ─────────────────────────────────────────────────────────────────────────────

/*
// === 使用示例：PureColor3D 材质 ===

// Step 1: 定义纯业务逻辑（S_PureColor3D.h）
constexpr char PURE_COLOR_VERTEX_LOGIC[] = R"(
    vec4 VertexMain(vec3 Position, uint MaterialInstanceID) {
        MaterialInstance mi = GetMI();  // 调用框架函数
        Output.Color = mi.Color;        // 输出到下一阶段
        return vec4(Position, 1.0);     // 返回本地坐标（框架会自动变换）
    }
)";

constexpr char PURE_COLOR_FRAGMENT_LOGIC[] = R"(
    vec4 FragmentMain() {
        return Input.Color;  // 直接使用上一阶段输出
    }
)";

// Step 2: 声明依赖
constexpr const char* VERTEX_REQUIRED_RESOURCES[] = {
    "MaterialInstanceData",
    "LocalToWorld"
};

constexpr const char* VERTEX_REQUIRED_HELPERS[] = {
    BuiltinHelpers::GetMI,
    BuiltinHelpers::GetWorldPos
};

// Step 3: 构造逻辑定义
MaterialLogicDef PURE_COLOR_LOGIC = {
    .vertex = {
        .main_logic = PURE_COLOR_VERTEX_LOGIC,
        .custom_functions = nullptr,
        .required_resources = VERTEX_REQUIRED_RESOURCES,
        .required_resource_count = 2,
        .required_helpers = VERTEX_REQUIRED_HELPERS,
        .required_helper_count = 2
    },
    .fragment = {
        .main_logic = PURE_COLOR_FRAGMENT_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    }
};

// Step 4: 与资源布局配合使用
// FixedMaterialDef 依然定义 vertex_entries/descriptors（资源布局）
// MaterialLogicDef 定义业务逻辑
// ComposedShaderGenerator 合并两者生成最终 GLSL

*/

} // namespace hgl::graph::mtl
