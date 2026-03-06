/// Stage1_ShaderLogic_Demo.cpp — 演示 ShaderLogic 接口的使用
/// 
/// 这个文件展示 Stage 1.1 完成后的成果：
///   ✅ 清晰的接口定义 (ShaderLogic.h)
///   ✅ 分离的业务逻辑 (S_PureColor3D_Logic.h)
///   ✅ 为 Stage 1.2 (资源声明生成) 和 Stage 2 (统一生成器) 做好准备
///
/// 注意：这是演示代码，暂不编译到项目中

#include <hgl/shadergen/ShaderLogic.h>
#include "../src/ShaderGen/3d/S_PureColor3D_Logic.h"
#include <stdio.h>

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// 演示：如何使用 MaterialLogicDef
// ─────────────────────────────────────────────────────────────────────────────

void DemonstratePureColorLogic()
{
    printf("=== Stage 1.1 Demo: PureColor3D Logic Definition ===\n\n");
    
    // 获取材质逻辑定义
    const MaterialLogicDef& logic = PURE_COLOR_3D_LOGIC;
    
    // Vertex Shader 逻辑
    printf("Vertex Shader Logic:\n");
    printf("  Main Logic:\n%s\n", logic.vertex.main_logic);
    printf("  Required Resources (%u): ", logic.vertex.required_resource_count);
    for (uint32_t i = 0; i < logic.vertex.required_resource_count; ++i) {
        printf("%s%s", logic.vertex.required_resources[i], 
               i < logic.vertex.required_resource_count - 1 ? ", " : "");
    }
    printf("\n");
    
    printf("  Required Helpers (%u): ", logic.vertex.required_helper_count);
    for (uint32_t i = 0; i < logic.vertex.required_helper_count; ++i) {
        printf("%s%s", logic.vertex.required_helpers[i],
               i < logic.vertex.required_helper_count - 1 ? ", " : "");
    }
    printf("\n\n");
    
    // Fragment Shader 逻辑
    printf("Fragment Shader Logic:\n");
    printf("  Main Logic:\n%s\n", logic.fragment.main_logic);
    printf("  Required Resources: %u\n", logic.fragment.required_resource_count);
    printf("  Required Helpers: %u\n\n", logic.fragment.required_helper_count);
}

// ─────────────────────────────────────────────────────────────────────────────
// 演示：对比旧方式 vs 新方式
// ─────────────────────────────────────────────────────────────────────────────

void PrintComparison()
{
    printf("=== Old Way vs New Way ===\n\n");
    
    printf("OLD (S_PureColor3D.h - PURE_COLOR_3D_COMPOSED_DEF):\n");
    printf("❌ Business code contains layout declarations:\n");
    printf("   layout(set=0,binding=0) buffer MaterialInstanceData {...} mtl;\n");
    printf("   MaterialInstance GetMI() { return mtl.mi[MaterialInstanceID]; }\n\n");
    
    printf("❌ Business code manually calls GetMI():\n");
    printf("   vec4 VertexShaderBusiness(vec3 Position, uint MaterialInstanceID) {\n");
    printf("       MaterialInstance mi = GetMI();\n");
    printf("       ...\n");
    printf("   }\n\n");
    
    printf("❌ Generator ALSO generates layout from FixedDescriptorEntry:\n");
    printf("   layout(set=0,binding=3) buffer MaterialInstanceData {...} mtl;\n\n");
    
    printf("❌ Result: Duplicate block name → Compilation FAILED!\n\n");
    
    printf("─────────────────────────────────────────────────────\n\n");
    
    printf("NEW (S_PureColor3D_Logic.h - PURE_COLOR_3D_LOGIC):\n");
    printf("✅ Business code contains ONLY computation:\n");
    printf("   vec4 VertexMain(vec3 Position, MaterialInstance mi) {\n");
    printf("       Output.Color = mi.Color;  // Use parameter directly\n");
    printf("       return vec4(Position, 1.0);\n");
    printf("   }\n\n");
    
    printf("✅ Framework generates main() that prepares data:\n");
    printf("   void main() {\n");
    printf("       MaterialInstance mi = GetMI();  // Framework calls\n");
    printf("       vec4 result = VertexMain(Position, mi);  // Pass to business\n");
    printf("       gl_Position = ApplyTransform(result);\n");
    printf("   }\n\n");
    
    printf("✅ Layout generated ONCE by ResourceLayoutGenerator\n");
    printf("✅ Result: No duplication, cleaner business code → Success!\n\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// 演示：Stage 1.2 的下一步计划
// ─────────────────────────────────────────────────────────────────────────────

void PrintNextSteps()
{
    printf("=== Next Steps (Stage 1.2) ===\n\n");
    
    printf("1. 修改其他材质定义为纯逻辑形式:\n");
    printf("   [ ] S_BasicLit.h → S_BasicLit_Logic.h\n");
    printf("   [ ] S_VertexColor3D.h → S_VertexColor3D_Logic.h\n");
    printf("   [ ] S_TextureBlinnPhong.h → S_TextureBlinnPhong_Logic.h\n");
    printf("   [ ] ...\n\n");
    
    printf("2. 实现 ResourceLayoutGenerator (Stage 2):\n");
    printf("   - GenDescriptorLayout(FixedDescriptorEntry[], count)\n");
    printf("   - GenVertexInputLayout(FixedVertexEntry[], count)\n");
    printf("   - 检查重复 binding，防止冲突\n\n");
    
    printf("3. 实现 BuiltinHelpers 辅助函数库 (Stage 3):\n");
    printf("   - GetMI() 的 GLSL 代码生成\n");
    printf("   - GetWorldPos() 的 GLSL 代码生成\n");
    printf("   - 根据 permutation key 选择性注入\n\n");
    
    printf("4. 重构 ShaderComposition 组装管线:\n");
    printf("   - 使用 MaterialLogicDef + FixedMaterialDef\n");
    printf("   - 调用 ResourceLayoutGenerator 生成布局\n");
    printf("   - 调用 BuiltinHelpers 注入辅助函数\n");
    printf("   - 插入业务逻辑\n");
    printf("   - 生成 main() 入口\n\n");
}

} // namespace hgl::graph::mtl

// ─────────────────────────────────────────────────────────────────────────────
// Main (演示入口)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef STAGE1_DEMO_STANDALONE
int main()
{
    using namespace hgl::graph::mtl;
    
    DemonstratePureColorLogic();
    printf("\n");
    PrintComparison();
    printf("\n");
    PrintNextSteps();
    
    return 0;
}
#endif

/*
=== Stage 1.1 成果总结 ===

已完成：
✅ 创建 ShaderLogic.h - 定义清晰的接口
✅ 创建 S_PureColor3D_Logic.h - PureColor3D 的纯逻辑实现
✅ 编译验证通过
✅ 文档和示例完整

核心设计：
✅ ShaderLogicBlock: 纯业务逻辑块（无 layout）
✅ MaterialLogicDef: 完整材质逻辑定义（Vertex + Fragment）
✅ ResourceDependency: 资源依赖声明（只是名字）
✅ BuiltinHelpers: 框架辅助函数命名空间

下一步：
→ Stage 1.2: 将现有材质改写为纯逻辑形式
→ Stage 2: 实现 ResourceLayoutGenerator
→ Stage 3: 实现 BuiltinHelpers 函数库

工期：
Stage 1.1: ✅ 完成 (30分钟)
Stage 1.2: 预计 2 小时
Stage 2: 预计 3 小时
Stage 3: 预计 2 小时

*/
