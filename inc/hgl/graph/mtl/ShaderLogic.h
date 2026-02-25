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
///       vec4 VertexMain(vec3 Position, MaterialInstance mi) {
///           Output.Color = mi.Color;
///           return vec4(Position, 1.0);  // 框架自动应用变换
///       }
///   )";
///
///   // 材质定义
///   ShaderLogicBlock vertex_logic = {
///       .main_logic = PURE_COLOR_VERTEX_LOGIC,
///       .required_resources = {"MaterialInstanceData", "LocalToWorld"},
///       .required_helpers = {}  // 基础数据由框架自动传入，不需要手动调用
///   };

#pragma once

#include <hgl/type/String.h>

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
 * 业务逻辑可能调用框架提供的高级函数（如光照计算、特效等）
 * 
 * 注意：基础数据（MaterialInstance 等）框架会自动提取并作为参数传入，
 *      不需要在这里声明依赖，也不需要在业务代码中手动调用 GetMI() 等
 */
struct HelperFunctionDependency {
    const char* name;           // 函数名，如 "ComputeLighting", "ApplyFog"
    const char* category;       // 类别：transform/lighting/output/utility
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
 *   - required_helpers: 需要哪些高级辅助函数（只是名字列表）
 * 
 * 框架职责：
 *   1. 根据 required_resources 生成所有 layout(...) 声明
 *   2. 从资源中提取数据（如调用 GetMI() 获取 MaterialInstance）
 *   3. 生成 main() 函数，在其中：
 *      - 提取基础数据作为参数
 *      - 调用业务函数: vec4 result = VertexMain(Position, mi);
 *      - 应用变换: gl_Position = ViewProj * LocalToWorld * result;
 *   4. 根据 required_helpers 注入高级辅助函数代码
 *   5. 将 custom_functions 和 main_logic 插入到合适位置
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
    const char* const* required_resources;
    uint32_t required_resource_count;

    /// 需要的框架辅助函数（只是名字数组）
    /// 示例: {"GetMI", "GetWorldPos", "GetNormal"}
    const char* const* required_helpers;
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
 * 框架内置高级辅助函数
 * 业务代码可以直接调用，无需自己实现
 * 
 * 注意：基础数据获取（GetMI GetWorldPos 等）不在这里，框架会：
 *      1. 自动调用这些函数获取数据
 *      2. 将数据作为参数传给业务函数
 *      3. 业务代码无需手动调用
 * 
 * 这里只列出业务代码可能需要手动调用的高级函数
 */
namespace BuiltinHelpers {
    // 高级变换
    constexpr char TransformNormal[] = "TransformNormal";     // vec3 TransformNormal(vec3)
    constexpr char GetTangentSpace[] = "GetTangentSpace";     // mat3 GetTangentSpace()
    
    // 光照计算
    constexpr char ComputeLighting[] = "ComputeLighting";     // vec3 ComputeLighting(...)
    constexpr char ComputePBR[] = "ComputePBR";               // vec3 ComputePBR(...)
    constexpr char ComputeShadow[] = "ComputeShadow";         // float ComputeShadow(...)
    
    // 特效
    constexpr char ApplyFog[] = "ApplyFog";                   // vec3 ApplyFog(vec3 color, float depth)
    constexpr char ApplyAlpha[] = "ApplyAlpha";               // vec4 ApplyAlpha(vec3 color, float alpha)
}

// ─────────────────────────────────────────────────────────────────────────────
// 使用示例（文档）
// ─────────────────────────────────────────────────────────────────────────────

/*
// === 使用示例：PureColor3D 材质 ===

// Step 1: 定义纯业务逻辑（S_PureColor3D.h）
constexpr char PURE_COLOR_VERTEX_LOGIC[] = R"(
    vec4 VertexMain(vec3 Position, MaterialInstance mi) {
        Output.Color = mi.Color;        // 直接使用参数
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
    "MaterialInstanceData",  // 框架会自动调用 GetMI() 并传给 VertexMain
    "LocalToWorld"           // 用于坐标变换
};

// 不需要声明基础辅助函数依赖，框架自动处理
constexpr const char** VERTEX_REQUIRED_HELPERS = nullptr;

// Step 3: 构造逻辑定义
MaterialLogicDef PURE_COLOR_LOGIC = {
    .vertex = {
        .main_logic = PURE_COLOR_VERTEX_LOGIC,
        .custom_functions = nullptr,
        .required_resources = VERTEX_REQUIRED_RESOURCES,
        .required_resource_count = 2,
        .required_helpers = nullptr,  // 基础函数由框架自动处理
        .required_helper_count = 0
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
