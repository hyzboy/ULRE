# ShaderLogic 最小必填约束与验证规范 v1.0

本文档定义了 `ShaderLogicBlock` / `MaterialLogicDef` 的最小必填字段约束、错误消息格式与运行时校验规则。

**最后更新**：2026-02-28（文档口径统一，无规范变更）  
**适用范围**：Phase B 及后续所有材质开发  
**关联文档**：
- [SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md)
- [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)

---

## 1. ShaderLogicBlock 必填字段约束

### 1.1 最小必填字段

✅ **必须提供**：
```cpp
struct ShaderLogicBlock {
    const char* main_logic;              // ✅ 必须非空（业务核心逻辑）
    const char* custom_functions;        // ⚠️ 可选（nullptr 表示无自定义函数）
    
    const char* const* required_resources;     // ⚠️ 可选（nullptr 表示无资源依赖）
    uint32_t required_resource_count;          // 如果 required_resources == nullptr，必须为 0
    
    const char* const* required_helpers;       // ⚠️ 可选（nullptr 表示无 helper 依赖）
    uint32_t required_helper_count;            // 如果 required_helpers == nullptr，必须为 0
};
```

### 1.2 字段约束详解

| 字段                      | 必填? | 允许 nullptr? | 允许空字符串? | Count 字段约束                          |
|---------------------------|-------|---------------|----------------|----------------------------------------|
| `main_logic`              | ✅ 是 | ❌ 否         | ❌ 否          | N/A                                    |
| `custom_functions`        | ❌ 否 | ✅ 是         | ✅ 是          | N/A                                    |
| `required_resources`      | ❌ 否 | ✅ 是         | N/A            | 如果为 nullptr，count 必须为 0         |
| `required_resource_count` | ✅ 是 | N/A           | N/A            | 如果 > 0，required_resources 不能为 nullptr |
| `required_helpers`        | ❌ 否 | ✅ 是         | N/A            | 如果为 nullptr，count 必须为 0         |
| `required_helper_count`   | ✅ 是 | N/A           | N/A            | 如果 > 0，required_helpers 不能为 nullptr |

### 1.3 非法组合示例

❌ **错误 1：main_logic 为空**
```cpp
constexpr ShaderLogicBlock INVALID_LOGIC = {
    .main_logic = nullptr,  // ❌ 必须提供业务逻辑
    ...
};
// 错误：[ShaderLogic] main_logic cannot be null
```

❌ **错误 2：count 与数组指针不匹配**
```cpp
constexpr ShaderLogicBlock INVALID_LOGIC = {
    .main_logic = MY_LOGIC,
    .required_resources = nullptr,
    .required_resource_count = 2,  // ❌ 数组为 null 但 count > 0
    ...
};
// 错误：[ShaderLogic] required_resource_count is 2 but required_resources is nullptr
```

❌ **错误 3：数组存在但 count 为 0**
```cpp
constexpr const char* RESOURCES[] = {"camera", "mtl"};

constexpr ShaderLogicBlock INVALID_LOGIC = {
    .main_logic = MY_LOGIC,
    .required_resources = RESOURCES,
    .required_resource_count = 0,  // ❌ 数组非空但 count 为 0
    ...
};
// 警告：[ShaderLogic] required_resources is non-null but count is 0 (will be ignored)
```

### 1.4 最小合法示例

✅ **示例 1：无任何依赖（最小材质）**
```cpp
constexpr char SIMPLE_VS_LOGIC[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi) {
    return vec4(vi.Position, 1.0);
}
)";

constexpr ShaderLogicBlock SIMPLE_VS = {
    .main_logic = SIMPLE_VS_LOGIC,
    .custom_functions = nullptr,
    .required_resources = nullptr,
    .required_resource_count = 0,
    .required_helpers = nullptr,
    .required_helper_count = 0
};
```

✅ **示例 2：有资源依赖**
```cpp
constexpr const char* RESOURCES[] = {"mtl"};

constexpr ShaderLogicBlock FS_WITH_MI = {
    .main_logic = FS_LOGIC,
    .custom_functions = nullptr,
    .required_resources = RESOURCES,
    .required_resource_count = 1,  // 与数组元素数匹配
    .required_helpers = nullptr,
    .required_helper_count = 0
};
```

---

## 2. MaterialLogicDef 必填约束

### 2.1 结构体定义

```cpp
struct MaterialLogicDef {
    VertexShaderLogic vertex;         // ✅ 必须提供（继承 ShaderLogicBlock）
    FragmentShaderLogic fragment;     // ✅ 必须提供（继承 ShaderLogicBlock）
    
    ShaderLogicBlock* geometry;       // ⚠️ 可选（nullptr 表示无 GS）
    ShaderLogicBlock* tess_control;   // ⚠️ 可选（nullptr 表示无 Tess Control）
    ShaderLogicBlock* tess_eval;      // ⚠️ 可选（nullptr 表示无 Tess Eval）
};
```

### 2.2 约束规则

| 字段           | 必填? | 允许 main_logic = nullptr? | 用途                     |
|----------------|-------|----------------------------|--------------------------|
| `vertex`       | ✅ 是 | ❌ 否                      | Vertex Shader 核心逻辑   |
| `fragment`     | ✅ 是 | ❌ 否                      | Fragment Shader 核心逻辑 |
| `geometry`     | ❌ 否 | ✅ 是（指针本身可为 nullptr）| Geometry Shader（可选）  |
| `tess_control` | ❌ 否 | ✅ 是（指针本身可为 nullptr）| Tessellation Control（可选）|
| `tess_eval`    | ❌ 否 | ✅ 是（指针本身可为 nullptr）| Tessellation Evaluation（可选）|

### 2.3 非法组合示例

❌ **错误：vertex.main_logic 为空**
```cpp
constexpr MaterialLogicDef INVALID_MAT = {
    .vertex = {
        .main_logic = nullptr,  // ❌ VS 必须提供逻辑
        ...
    },
    .fragment = { ... }
};
// 错误：[MaterialLogicDef] vertex.main_logic cannot be null
```

❌ **错误：fragment.main_logic 为空**
```cpp
constexpr MaterialLogicDef INVALID_MAT = {
    .vertex = { .main_logic = VS_LOGIC, ... },
    .fragment = {
        .main_logic = nullptr,  // ❌ FS 必须提供逻辑
        ...
    }
};
// 错误：[MaterialLogicDef] fragment.main_logic cannot be null
```

✅ **合法：不使用 Geometry Shader**
```cpp
constexpr MaterialLogicDef VALID_MAT = {
    .vertex = { .main_logic = VS_LOGIC, ... },
    .fragment = { .main_logic = FS_LOGIC, ... },
    .geometry = nullptr,  // ✅ GS 可选
    ...
};
```

---

## 3. 错误消息格式规范

### 3.1 错误消息分级

| 级别   | 前缀       | 触发条件                        | 行为               |
|--------|------------|---------------------------------|--------------------|
| Error  | `[Error]`  | 违反必填约束（如 main_logic = nullptr） | 编译失败，拒绝生成 |
| Warning| `[Warning]`| 潜在问题（如 count 与数组不匹配）      | 编译继续，输出警告 |
| Info   | `[Info]`   | 诊断信息（如资源注入成功）            | 调试日志          |

### 3.2 错误消息模板

#### 3.2.1 必填字段为空

```
[Error] {结构体类型}.{字段名} cannot be null
  Material: {材质名称}
  Stage: {Shader Stage}
  Context: {定义文件:行号}
  Fix: Provide a non-null {字段类型} string
```

**示例**：
```
[Error] ShaderLogicBlock.main_logic cannot be null
  Material: PureColor3D
  Stage: VertexShader
  Context: S_PureColor3D.h:42
  Fix: Provide a non-null GLSL string for vertex business logic
```

#### 3.2.2 Count 与数组不匹配

```
[Error] {数组字段名}_count is {count} but {数组字段名} is nullptr
  Material: {材质名称}
  Stage: {Shader Stage}
  Fix: Set {数组字段名}_count = 0 OR provide non-null array
```

**示例**：
```
[Error] required_resource_count is 2 but required_resources is nullptr
  Material: VertexColor3D
  Stage: FragmentShader
  Fix: Set required_resource_count = 0 OR provide non-null resource array
```

#### 3.2.3 资源未找到

```
[Error] Required resource '{资源名}' not found in material descriptors
  Material: {材质名称}
  Stage: {Shader Stage}
  Available resources: {descriptor_name1, descriptor_name2, ...}
  Fix: Add descriptor {{ .name = "{资源名}", ... }} to material definition
```

**示例**：
```
[Error] Required resource 'camera' not found in material descriptors
  Material: Gizmo3D
  Stage: VertexShader
  Available resources: mtl, l2w
  Fix: Add descriptor { .name = "camera", .type = SET_Uniform, .stage = STAGE_Vertex }
```

#### 3.2.4 Helper 函数未找到

```
[Warning] Required helper '{helper名}' not found in BuiltinHelpers registry
  Material: {材质名称}
  Stage: {Shader Stage}
  Context: Helper will be treated as custom function (must be provided in custom_functions)
```

**示例**：
```
[Warning] Required helper 'GetWorldPos' not found in BuiltinHelpers registry
  Material: Gizmo3D
  Stage: VertexShader
  Context: Helper will be treated as custom function (must be provided in custom_functions)
```

### 3.3 成功消息格式

```
[Info] ShaderLogic validated successfully
  Material: {材质名称}
  Vertex resources: {count} ({res1, res2, ...})
  Fragment resources: {count} ({res1, res2, ...})
  Helpers: {count} ({helper1, helper2, ...})
```

**示例**：
```
[Info] ShaderLogic validated successfully
  Material: VertexColor3D
  Vertex resources: 2 (camera, l2w)
  Fragment resources: 1 (mtl)
  Helpers: 0
```

---

## 4. 运行时校验规则

### 4.1 校验时机

| 阶段                  | 校验内容                                | 失败行为               |
|-----------------------|-----------------------------------------|------------------------|
| 材质定义时（编译期）  | 结构体字段完整性（必填字段非空）        | C++ 编译错误           |
| 材质编译时（运行时）  | 资源依赖合法性（resource 名称存在于 descriptors） | 返回 nullptr，输出错误 |
| Shader 生成时         | helper 注入冲突检测（无重复定义）       | 返回 nullptr，输出错误 |
| Shader 编译时         | GLSL 语法正确性（由驱动检查）           | fallback 到 legacy 路径|

### 4.2 校验函数伪代码

```cpp
bool ValidateShaderLogicBlock(const ShaderLogicBlock& logic, const char* stage_name)
{
    // 1. 检查必填字段
    if (!logic.main_logic || strlen(logic.main_logic) == 0) {
        Error("[Error] %s.main_logic cannot be null or empty\n", stage_name);
        return false;
    }
    
    // 2. 检查 count 与数组匹配
    if (logic.required_resource_count > 0 && !logic.required_resources) {
        Error("[Error] %s.required_resource_count is %u but required_resources is nullptr\n",
              stage_name, logic.required_resource_count);
        return false;
    }
    
    if (logic.required_resources && logic.required_resource_count == 0) {
        Warning("[Warning] %s.required_resources is non-null but count is 0 (ignored)\n", stage_name);
    }
    
    // 3. 检查 helper count match
    if (logic.required_helper_count > 0 && !logic.required_helpers) {
        Error("[Error] %s.required_helper_count is %u but required_helpers is nullptr\n   stage_name, logic.required_helper_count);
        return false;
    }
    
    return true;
}

bool ValidateMaterialLogicDef(const MaterialLogicDef& def, const char* material_name)
{
    bool ok = true;
    
    // Vertex Shader 是必须的
    if (!ValidateShaderLogicBlock(def.vertex, "VertexShader")) {
        Error("[Error] %s: Vertex logic validation failed\n", material_name);
        ok = false;
    }
    
    // Fragment Shader 是必须的
    if (!ValidateShaderLogicBlock(def.fragment, "FragmentShader")) {
        Error("[Error] %s: Fragment logic validation failed\n", material_name);
        ok = false;
    }
    
    // Geometry Shader 可选
    if (def.geometry && !ValidateShaderLogicBlock(*def.geometry, "GeometryShader")) {
        Error("[Error] %s: Geometry logic validation failed\n", material_name);
        ok = false;
    }
    
    return ok;
}
```

### 4.3 调用时机

```cpp
// 材质编译入口
MaterialCreateInfo* CompileComposedBusinessMaterial(
    const VulkanDevAttr* dev_attr,
    const FixedMaterialDef& def,
    const ComposedMaterialDef& composed_def,
    const MaterialLogicDef& logic,  // 业务逻辑
    const ShaderPermutationKey& key,
    const Material3DCreateConfig* config)
{
    // 先校验 logic 定义的合法性
    if (!ValidateMaterialLogicDef(logic, def.name)) {
        std::fprintf(stderr, "[CompileComposedBusinessMaterial] %s: Logic validation failed, aborting\n", def.name);
        return nullptr;
    }
    
    // 校验资源依赖
    if (!ValidateResourceDependencies(logic, def)) {
        std::fprintf(stderr, "[CompileComposedBusinessMaterial] %s: Resource dependency validation failed\n", def.name);
        return nullptr;
    }
    
    // 继续编译...
}
```

---

## 5. Phase B 验收标准

### 5.1 已迁移材质验证

- [ ] PureColor3D 通过 `ValidateMaterialLogicDef`
- [ ] VertexColor3D 通过 `ValidateMaterialLogicDef`
- [ ] Gizmo3D（如已迁移）通过 `ValidateMaterialLogicDef`

### 5.2 错误消息测试

创建故意错误的材质定义，确保错误消息正确输出：

```cpp
// 测试用例 1：main_logic 为 nullptr
constexpr ShaderLogicBlock TEST_NULL_LOGIC = {
    .main_logic = nullptr,  // 故意错误
    .custom_functions = nullptr,
    .required_resources = nullptr,
    .required_resource_count = 0,
    ...
};
// 预期输出：[Error] ShaderLogicBlock.main_logic cannot be null

// 测试用例 2：count 不匹配
constexpr ShaderLogicBlock TEST_COUNT_MISMATCH = {
    .main_logic = "...",
    .required_resources = nullptr,
    .required_resource_count = 5,  // 故意错误
    ...
};
// 预期输出：[Error] required_resource_count is 5 but required_resources is nullptr
```

### 5.3 文档更新

- [ ] 在 `inc/hgl/graph/mtl/ShaderLogic.h` 头部增加约束说明注释
- [ ] 所有 `S_*.h` 材质定义文件遵守最小必填约束
- [ ] 编译器代码增加 `ValidateMaterialLogicDef` 调用

---

## 6. 实现建议

### 6.1 增加编译期静态检查宏

```cpp
// ShaderLogic.h
#define REQUIRE_NON_NULL(field, name) \
    static_assert((field) != nullptr, name " cannot be null at compile time")

// 使用示例（S_PureColor3D.h）
constexpr char PURE_COLOR_VS_LOGIC[] = R"(...)";
REQUIRE_NON_NULL(PURE_COLOR_VS_LOGIC, "PURE_COLOR_VS_LOGIC");

constexpr ShaderLogicBlock PURE_COLOR_VS = {
    .main_logic = PURE_COLOR_VS_LOGIC,
    ...
};
```

**优势**：编译期即可检测错误，无需等到运行时。

### 6.2 增加运行时诊断API

```cpp
// inc/hgl/graph/mtl/ShaderLogic.h
namespace hgl::graph::mtl {

/// 打印材质逻辑诊断信息（调试用）
void PrintMaterialLogicDiagnostics(const MaterialLogicDef& logic, const char* material_name);

/// 示例输出：
/// [Diagnostics] Material: PureColor3D
///   Vertex:
///     main_logic: 120 chars
///     custom_functions: nullptr
///     required_resources: 2 (camera, l2w)
///     required_helpers: 0
///   Fragment:
///     main_logic: 45 chars
///     required_resources: 1 (mtl)
///     required_helpers: 0

}
```

---

## 7. 变更历史

| 版本 | 日期       | 变更内容                                           |
|------|------------|----------------------------------------------------|
| 1.0  | 2026-02-26 | 初版发布，规范 ShaderLogicBlock / MaterialLogicDef 约束与错误格式 |
| 1.1  | 2026-02-28 | 文档口径同步：更新时间与阶段状态说明对齐（无规范变更） |

---

**责任人**：Shader System 维护团队  
**审核周期**：Phase B 完成后冻结  
**相关文档**：
- [SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md)
- [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)
