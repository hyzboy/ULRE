/// S_PureColor3D_Logic.h — PureColor3D 材质的纯业务逻辑定义
/// 
/// 这是 Stage 1 重构后的新写法示例
/// 对比旧的 S_PureColor3D.h，这里：
///   ✅ 没有任何 layout(...) 声明
///   ✅ 只包含纯计算逻辑
///   ✅ 资源通过名字引用，由框架生成声明

#pragma once

#include <hgl/shadergen/ShaderLogic.h>

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// 业务逻辑代码（纯GLSL函数，无资源声明）
// ─────────────────────────────────────────────────────────────────────────────

/// Vertex Shader 主逻辑
/// 
/// 输入参数：（框架自动准备并传入）
///   - Position: vec3 (来自 VBO)
///   - mi: MaterialInstance (框架已从 MaterialInstanceData 中提取)
/// 
/// 输出：
///   - Output.Color: 传递给 Fragment Shader
///   - 返回值: 本地坐标（框架会自动应用 LocalToWorld 和 ViewProj 变换）
/// 
/// 设计优势：
///   ✅ 业务代码不需要知道如何获取 MaterialInstance
///   ✅ 框架在 main() 中调用 GetMI() 并传给业务函数
///   ✅ 业务函数签名清晰，只做纯计算
constexpr char PURE_COLOR_VERTEX_LOGIC[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    return vec4(vi.Position, 1.0);
}
)";

/// Fragment Shader 主逻辑
/// 
/// 输入：
///   - 通过全局 Input (Vertex_Output 接口块) 访问 VS 传递的数据（本材质未使用）
///   - 通过 GetMI() 访问材质实例数据
/// 
/// 输出：
///   - vec4: 最终颜色（写入 FragColor 由框架 main 完成）
constexpr char PURE_COLOR_FRAGMENT_LOGIC[] = R"(
vec4 FragmentShaderBusiness()
{
    MaterialInstance mi = GetMI();
    return mi.Color;
}
)";

// ─────────────────────────────────────────────────────────────────────────────
// 资源依赖声明（只是名字，不是 layout）
// ─────────────────────────────────────────────────────────────────────────────

/// Vertex Shader 需要的资源
constexpr const char* PURE_COLOR_VERTEX_RESOURCES[] = {
    "MaterialInstanceData",     // Material Instance 缓冲
    "l2w"                       // 变换矩阵（descriptor name）
};

/// Vertex Shader 需要的高级辅助函数（基础数据提取由框架自动处理）
/// PureColor3D 很简单，不需要任何高级辅助函数
constexpr const char* const* PURE_COLOR_VERTEX_HELPERS = nullptr;

/// Fragment Shader 不需要额外资源（使用来自 VS 的插值）
constexpr const char* PURE_COLOR_FRAGMENT_RESOURCES[] = {
    "MaterialInstanceData"
};
constexpr const char* const* PURE_COLOR_FRAGMENT_HELPERS = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// 完整材质逻辑定义
// ─────────────────────────────────────────────────────────────────────────────

/// PureColor3D 材质的纯逻辑定义
/// 
/// 配合使用：
///   - PURE_COLOR_3D_DEF (FixedMaterialDef): 定义资源布局
///   - PURE_COLOR_3D_LOGIC (MaterialLogicDef): 定义业务逻辑
/// 
/// 框架会：
///   1. 从 FixedMaterialDef 生成 layout(...) 声明
///   2. 从 MaterialLogicDef 注入业务逻辑和辅助函数
///   3. 合成完整的 GLSL
const VertexShaderLogic PURE_COLOR_VERTEX_SHADER_LOGIC = {
    {
        PURE_COLOR_VERTEX_LOGIC,
        nullptr,
        PURE_COLOR_VERTEX_RESOURCES,
        2,
        PURE_COLOR_VERTEX_HELPERS,
        0
    }
};

const FragmentShaderLogic PURE_COLOR_FRAGMENT_SHADER_LOGIC = {
    {
        PURE_COLOR_FRAGMENT_LOGIC,
        nullptr,
        PURE_COLOR_FRAGMENT_RESOURCES,
        1,
        nullptr,
        0
    }
};

const MaterialLogicDef PURE_COLOR_3D_LOGIC = {
    PURE_COLOR_VERTEX_SHADER_LOGIC,
    PURE_COLOR_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

// ─────────────────────────────────────────────────────────────────────────────
// 使用说明（对比旧方式）
// ─────────────────────────────────────────────────────────────────────────────

/*
### 旧方式（S_PureColor3D.h 中的 PURE_COLOR_3D_COMPOSED_DEF）

问题：
❌ 业务代码包含 layout(...) 声明：
    layout(set=0,binding=0) readonly buffer MaterialInstanceData { ... } mtl;
    
❌ 业务代码需要手动调用 GetMI()：
    vec4 VertexShaderBusiness(vec3 Position, uint MaterialInstanceID) {
        MaterialInstance mi = GetMI();  // 手动调用
        ...
    }

❌ Generator 又基于 FixedDescriptorEntry 生成声明：
    layout(set=0, binding=3, std430) buffer MaterialInstanceData { ... } mtl;

❌ 结果：重复的 block name，编译失败！


### 新方式（S_PureColor3D_Logic.h — 本文件）

优势：
✅ 业务代码只有纯计算：
    vec4 VertexMain(vec3 Position, MaterialInstance mi) {
        Output.Color = mi.Color;  // 直接使用参数
    }

✅ 框架生成的 main() 负责准备数据：
    void main() {
        MaterialInstance mi = GetMI();  // 框架调用
        vec4 result = VertexMain(Position, mi);  // 传给业务
        gl_Position = ApplyTransform(result);
    }

✅ MaterialInstanceData 的 layout 由 ResourceLayoutGenerator 统一生成

✅ 无重复，无冲突，业务代码更纯粹！


### 下一步（Stage 1.2）

将所有材质的业务代码改写成这种纯逻辑形式：
- S_PureColor3D.h → S_PureColor3D_Logic.h ✅ (本文件)
- S_BasicLit.h → S_BasicLit_Logic.h
- S_PBR.h → S_PBR_Logic.h
- ...
*/

} // namespace hgl::graph::mtl
