# Shader System 渐进式重构计划

## 现状分析

### 当前系统模块拆解

```
┌─────────────────────────────────────────────────────────────┐
│                    Material Factory                          │
│  (M_PureColor3D.cpp, M_BasicLit3D.cpp, etc.)                │
└───────────────┬─────────────────────────────────────────────┘
                │
                ├──► Old Path (现在在用)
                │   └─► Std3DMaterial (基类)
                │       └─► CompileStdMaterial()
                │           └─► ShaderCreateInfo
                │               └─► SPIRV Compiler
                │
                └──► New Path (M0.5刚做的)
                    └─► ComposedMaterialDef
                        └─► ComposedShaderGenerator
                            └─► 生成GLSL (有重复声明bug)
                                └─► CompileFixedMaterial()
                                    └─► ShaderCreateInfo
                                        └─► SPIRV Compiler
```

### 核心问题定位

1. **问题A：业务逻辑混杂资源声明**
   - `VertexShaderBusiness` 代码块包含 `layout(...)` 声明
   - `ComposedShaderGenerator` 又基于 `FixedDescriptorEntry` 生成声明
   - **结果：重复冲突**

2. **问题B：两套材质系统并存**
   - `Std3DMaterial` 子类化方式（老）
   - `ComposedMaterialDef` 组合方式（新）
   - **结果：迁移困难**

3. **问题C：GLSL生成与编译耦合**
   - Generator生成的GLSL必须匹配ShaderCreateInfo期望
   - ShaderCreateInfo自己加#version，Generator也想控制
   - **结果：集成脆弱**

---

## 渐进式重构路线图

### 阶段0：止血 — 让现有材质继续工作 ✅
**目标**: 保证已有材质(PureColor/BasicLit等)能正常渲染  
**行动**: 暂时回退M_PureColor3D.cpp到Std3DMaterial方式  
**验证**: 运行示例能看到正确渲染  
**工期**: 10分钟

---

### 阶段1：解耦业务逻辑与资源声明 (核心！)
**目标**: 让业务代码只包含计算逻辑，资源声明独立管理

#### Step 1.1: 定义纯逻辑接口 ✅ COMPLETED
```cpp
// 新文件: inc/hgl/graph/mtl/ShaderLogic.h

// 纯计算逻辑，不包含任何 layout(...) 声明
struct ShaderLogicBlock {
    UTF8String glsl_functions;     // 辅助函数（GetMI(), GetWorldPos()等）
    UTF8String main_logic;         // 主逻辑代码
    
    // 依赖的资源（只是名字，不是声明）
    List<UTF8String> required_uniforms;   // ["ViewProj", "LocalToWorld"]
    List<UTF8String> required_buffers;    // ["MaterialInstanceData"]
    List<UTF8String> required_inputs;     // ["Position", "Normal"]
};
```

#### Step 1.2: 修改现有业务代码 ✅ COMPLETED
**目标**: 将典型材质改写为纯逻辑形式，验证接口的实用性和灵活性

**已完成的材质**:
- [x] PureColor3D → S_PureColor3D_Logic.h（使用MaterialInstance）
- [x] VertexColor3D → S_VertexColor3D_Logic.h（最简单，只传递顶点属性）
- [x] Gizmo3D → S_Gizmo3D_Logic.h（带光照计算，展示高级辅助函数需求）

**覆盖的场景**:
- ✅ 使用材质实例的材质（PureColor3D）
- ✅ 不使用材质实例的材质（VertexColor3D）
- ✅ 需要高级辅助函数的材质（Gizmo3D - 法线变换、相机位置等）
- ✅ 自定义光照计算（Gizmo3D - Blinn-Phong）

**验证**: 编译通过 ✅

**结论**: 接口设计灵活且实用，可以覆盖从简单到复杂的各种材质需求

**工期**: 45分钟（原计划2小时，因为只需要典型示例）
**Before (S_PureColor3D.h)**:
```cpp
constexpr char PURE_COLOR_VERTEX_LOGIC[] = R"(
layout(set=0,binding=0) buffer MaterialInstanceData { ... } mtl;  // ❌ 有layout
vec4 VertexShaderBusiness() { ... }
)";
```

**After**:
```cpp
constexpr char PURE_COLOR_VERTEX_LOGIC[] = R"(
// 只有纯逻辑，假设资源已声明 ✅
vec4 VertexShaderBusiness(vec3 Position, uint MaterialInstanceID) {
    MaterialInstance mi = GetMI();  // GetMI()由框架提供
    return vec4(Position, 1.0);
}
)";
```

**验证**: 手动检查所有`S_*.h`业务代码，确保无layout声明  
**工期**: 2小时（需要改PureColor/BasicLit/PBR等所有材质定义）

---

### 阶段2：统一资源声明生成
**目标**: 所有layout声明由一个地方生成，避免重复

#### Step 2.1: 资源声明生成器
```cpp
// 新文件: src/ShaderGen/ResourceLayoutGenerator.cpp

class ResourceLayoutGenerator {
public:
    UTF8String GenDescriptorLayout(const FixedDescriptorEntry* entries, uint32_t count);
    UTF8String GenVertexInputLayout(const FixedVertexEntry* entries, uint32_t count);
    UTF8String GenOutputLayout(const ShaderStageOutputDef& output);
    
private:
    void CheckDuplicateBinding(uint32_t set, uint32_t binding);  // 防重复！
};
```

#### Step 2.2: 重构ComposedShaderGenerator
```cpp
// 修改: src/ShaderGen/ComposedShaderGenerator.cpp

UTF8String ComposeVertexShader(const ComposedMaterialDef& def) {
    UTF8String result;
    
    // 1. Preamble (#version, #define)
    result += GenPreamble();
    
    // 2. 资源声明（统一生成，检查重复）
    ResourceLayoutGenerator layout_gen;
    result += layout_gen.GenVertexInputLayout(def.vertex_entries, def.vertex_count);
    result += layout_gen.GenDescriptorLayout(def.descriptors, def.descriptor_count);
    
    // 3. 辅助函数（框架提供）
    result += GenHelperFunctions(def.permutation_key);
    
    // 4. 业务逻辑（不含layout）
    result += def.vertex_business.glsl_functions;
    result += def.vertex_business.main_logic;
    
    // 5. main()入口
    result += GenMainFunction();
    
    return result;
}
```

**验证**: 生成的GLSL不再有重复声明错误  
**工期**: 3小时

---

### 阶段3：框架辅助函数库
**目标**: GetMI()/GetWorldPos()等由框架统一提供，业务代码直接用

#### Step 3.1: 辅助函数代码库
```cpp
// 新文件: src/ShaderGen/BuiltinHelpers.cpp

namespace shader_helpers {
    // 根据permutation key决定生成哪些helper
    constexpr char HELPER_GET_MI[] = R"(
MaterialInstance GetMI() {
    return mtl.mi[MaterialInstanceID];
}
)";

    constexpr char HELPER_GET_WORLD_POS[] = R"(
vec4 GetWorldPos() {
    return l2w.matrices[TransformID] * vec4(Position, 1.0);
}
)";

    // ... 更多helpers
    
    UTF8String GenHelpers(const ShaderPermutationKey& key) {
        UTF8String result;
        if (key.需要材质实例) result += HELPER_GET_MI;
        if (key.需要变换) result += HELPER_GET_WORLD_POS;
        // ...
        return result;
    }
}
```

#### Step 3.2: 业务代码简化
**Before**:
```cpp
constexpr char VERTEX_CODE[] = R"(
MaterialInstance GetMI() { ... }  // 自己定义
vec4 GetWorldPos() { ... }
vec4 Business() { return GetWorldPos(); }
)";
```

**After**:
```cpp
constexpr char VERTEX_CODE[] = R"(
// 直接用框架的GetMI/GetWorldPos ✅
vec4 Business() { return GetWorldPos(); }
)";
```

**验证**: 所有材质能正确访问资源，无需重复定义helper  
**工期**: 2小时

---

### 阶段4：验证与测试
**目标**: 确保重构后系统能正常工作

#### Step 4.1: 单元测试
```cpp
TEST(ResourceLayoutGenerator, NoDuplicateBindings) {
    FixedDescriptorEntry entries[] = {
        {DST_LOCAL_TO_WORLD, DK_STORAGE_BUFFER, 0, "l2w"},
        {DST_MATERIAL_INSTANCE, DK_STORAGE_BUFFER, 0, "mtl"},  // 同一个set
    };
    
    ResourceLayoutGenerator gen;
    EXPECT_THROW(gen.GenDescriptorLayout(entries, 2), DuplicateBindingError);
}
```

#### Step 4.2: 集成测试
- 每个材质(PureColor/BasicLit/PBR)单独测试渲染
- 验证GLSL编译无错误
- 验证运行时渲染正确

**工期**: 2小时

---

### 阶段5：增强与优化（可选）
**目标**: 在稳定基础上添加新功能

- [ ] Shader缓存优化
- [ ] 更丰富的permutation支持
- [ ] 运行时shader热重载
- [ ] Shader graph可视化编辑

---

## 总时间估算
- 阶段0: 10分钟
- 阶段1: 2小时
- 阶段2: 3小时
- 阶段3: 2小时
- 阶段4: 2小时

**总计: ~10小时** (分3-4天完成，每步验证)

---

## 风险控制

### 每阶段验证标准
- ✅ 代码编译通过
- ✅ 至少一个材质能正确渲染
- ✅ 无GLSL编译错误
- ✅ Git提交，可回滚

### 回滚策略
每个阶段独立分支:
```bash
git checkout -b refactor/stage1-decouple-logic
# 完成Stage1
git commit -m "Stage1: Decouple business logic from resource declarations"
git checkout main
git merge refactor/stage1-decouple-logic  # 验证通过才合并
```

---

## 立即行动

### 现在开始 Stage 0: 止血
你想要我：
1. **先回退M_PureColor3D.cpp**，确保系统能跑？
2. **还是直接开始Stage 1.1**，定义新的ShaderLogicBlock接口？
3. **你有其他想法**？

告诉我从哪里开始！
