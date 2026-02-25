# 辅助函数自动生成设计文档

## 概述：消除不必要的 GLSL 样板代码

### 问题
原来系统的问题在于，开发者需要：
1. 记住 MFGetPosition.h 中有 8 种 GetPosition3D 变体
2. 记住 MFGetNormal.h 中有不同的 GetNormal_VS 和 GetNormal_Other
3. 记住 MFCommon.h 中 GetMI 在 VS/GS/FS 三个阶段都不同
4. 手工选择正确的变体，错误率高

### 解决方案
框架 `ComposedShaderGenerator` 根据：
- `ComposedMaterialDef` 中的顶点描述符（Position 有无、Normal 有无等）
- `ShaderPermutationKey` 中的坐标系设置（NDC / ZeroToOne / Ortho）
- Shader Stage 标记（VS / GS / FS）

**自动生成**唯一正确的函数实现，开发者直接调用同一套接口。

---

## 工作流程：从材质定义到生成的函数

### Example：BasicLit 材质自动生成过程

```cpp
// 1. 开发者定义材质（ComposedMaterialDef）
constexpr ComposedMaterialDef BASIC_LIT_COMPOSED {
    .vertex_entries = BASIC_LIT_VERTEX,      // 包含 Position, Normal, TexCoord
    .vertex_entry_count = 3,
    .descriptor_entries = BASIC_LIT_DESCRIPTORS,  // 包含 LocalToWorld (UBO), CameraInfo (UBO)
    // ...其他字段
};

// 2. 框架调用 ComposeVertexShader()
AnsiString vs = ComposedShaderGenerator::ComposeVertexShader(BASIC_LIT_COMPOSED, key);

// 3. 框架内部：
//    a) 调用 GenHelperFunctionLibrary(BASIC_LIT_COMPOSED, "VS");
//    b) 该函数返回包含以下内容的完整代码块：
//       - GetLocalToWorld()
//       - GetNormalMatrix()
//       - GetNormal(vec3 local_normal)  ← VS 版本（接收参数）
//       - GetPosition3D()  ← VS 版本（使用 Position input）
//       - GetClipPosition()
//       - GetMaterialInstance()
//
//    c) 框架拼接：前置 + 结构体 + 辅助函数库 + 业务代码 + main()
```

---

## 分类设计：GenHelperFunctionLibrary 实现

### 工作表

输入参数：
- `def`: ComposedMaterialDef（包含顶点输入、描述符、坐标系等）
- `shader_stage`: "VS" / "GS" / "FS"

输出：
- 完整的 GLSL 函数库字符串

### Case 1：顶点着色器 (VS) 的辅助函数

#### 背景
VS 可以直接访问顶点输入（Position, Normal, TexCoord 等）
VS 需要计算世界坐标、裁剪坐标、世界法线

#### 自动生成的函数

```glsl
// 来自描述符 LocalToWorld (UBO/SSBO)
mat4 GetLocalToWorld() {
    return l2w.mats[TransformID];  // TransformID 来自顶点输入 (Assign)
}

// 推导自 GetLocalToWorld + ViewMatrix
mat3 GetNormalMatrix() {
    return mat3(camera.view * GetLocalToWorld());  // 自动从描述符提取
}

// 接收顶点 local normal → 返回世界法线
vec3 GetNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}

// 直接用 VS 输入
vec3 GetNormal() {
    return normalize(GetNormalMatrix() * Normal);  // Normal 来自顶点属性
}

// 返回世界坐标
vec4 GetPosition3D() {
    return GetLocalToWorld() * vec4(Position, 1.0);  // Position 来自顶点属性
}

// 自动投影到裁剪坐标
vec4 GetClipPosition() {
    return camera.vp * GetPosition3D();  // camera.vp 来自描述符 CameraInfo
}

// 材质实例（从 SSBO 或 Assign 读取）
MaterialInstance GetMaterialInstance() {
    return mtl.mi[MaterialInstanceID];  // MaterialInstanceID 来自顶点属性
}

MaterialInstance GetMI() {
    return GetMaterialInstance();  // 别名
}
```

#### 决策树：何时生成哪个函数

```
生成 GetLocalToWorld():
  ✓ 总是生成（描述符中必有 LocalToWorld）

生成 GetNormalMatrix():
  ✓ 总是生成

生成 GetNormal(vec3):
  ✓ 总是生成

生成 GetNormal0(无参)：
  ✓ IF: 顶点输入中包含 Normal attribute
  ✗ ELSE: 不生成（开发者必须 GetNormal(法线值)）

生成 GetPosition3D():
  ✓ 总是生成（返回 LocalToWorld * Position）

生成 GetClipPosition():
  ✓ 当描述符中有 CameraInfo (VP 矩阵)

生成 GetMaterialInstance():
  ✓ 当描述符中有 MaterialInstanceData (SSBO)
  ✗ ELSE: 不生成，开发者手工构造

生成 HandoverMaterialInstanceID():
  ✗ VS 中不生成（VS 输出结构自动包含 MaterialInstanceID）
```

---

### Case 2：几何着色器 (GS) 的辅助函数

#### 特点
- GS 输入来自 VS 输出（所有顶点数据已插值）
- GS 需要访问多个输入顶点的属性
- GS 需要转发 MaterialInstanceID 到 FS

#### 自动生成的函数

```glsl
// GS 中的 GetLocalToWorld 需要特殊处理
// 因为可能收到多个顶点，每个顶点有不同的 TransformID

mat4 GetLocalToWorld(int vertex_index) {
    return l2w.mats[Input[vertex_index].TransformID];
}

// 法线矩阵：针对当前处理的顶点
mat3 GetNormalMatrix(int vertex_index) {
    return mat3(camera.view * GetLocalToWorld(vertex_index));
}

// 获取 GS 输入顶点的世界法线
vec3 GetWorldNormal(int vertex_index) {
    return Input[vertex_index].WorldNormal;  // 已从 VS 插值
}

// 获取 GS 输入顶点的材质实例
MaterialInstance GetMaterialInstance(int vertex_index) {
    return mtl.mi[Input[vertex_index].MaterialInstanceID];
}

// 转发 MaterialInstanceID 到 FS
void HandoverMaterialInstanceID(int vertex_index) {
    Output.MaterialInstanceID = Input[vertex_index].MaterialInstanceID;
    EmitVertex();
}
```

---

### Case 3：片元着色器 (FS) 的辅助函数

#### 特点
- FS 输入是从 VS (或 GS) 插值的 VS_Output
- FS 无法访问原始顶点属性（如 Position/Normal），只能用插值值
- FS 无法重新计算坐标变换（那是 VS 的责任）

#### 自动生成的函数

```glsl
// FS 中的世界坐标、法线来自插值
// 因此 GetPosition3D() / GetNormal() 返回的是插值值

vec4 GetPosition3D() {
    return vec4(Input.WorldPosition, 1.0);  // 从 Input 结构获取
}

vec3 GetWorldNormal() {
    return Input.WorldNormal;  // 从 Input 结构获取
}

// 向后兼容 GetNormal()
vec3 GetNormal() {
    return GetWorldNormal();
}

// 材质实例：从插值的 MaterialInstanceID 读取
MaterialInstance GetMaterialInstance() {
    return mtl.mi[Input.MaterialInstanceID];
}

MaterialInstance GetMI() {
    return GetMaterialInstance();
}

// FS 中不生成 GetLocalToWorld() / GetNormalMatrix()
// 因为已经在 VS 中处理好了，FS 只需要最终结果（世界坐标/法线）
```

---

## 参数化工作表

### 矩阵来源与获取方式

| 需求 | 来源 | 获取函数 | 描述符要求 |
|------|------|---------|----------|
| LocalToWorld | 顶点属性 TransformID | `l2w.mats[TransformID]` | LocalToWorld (UBO/SSBO) |
| ViewMatrix | 描述符 CameraInfo | `camera.view` | CameraInfo (UBO) |
| ProjectionMatrix | 描述符 CameraInfo | `camera.proj` | CameraInfo (UBO) |
| ViewProjection | 描述符 CameraInfo | `camera.vp` | CameraInfo (UBO) |
| NormalMatrix | 推导 | `mat3(view * GetLocalToWorld())` | LocalToWorld + CameraInfo |
| OrthoMatrix | 描述符 ViewportInfo | `viewport.ortho_matrix` | ViewportInfo (UBO) |

### 顶点属性与对应的自动函数

| 顶点属性 | 自动函数 | 签名 | 备注 |
|---------|---------|------|------|
| Position | GetPosition3D() | `vec4 GetPosition3D()` | VS: 乘以 L2W；GS/FS: 返回插值值 |
| Normal | GetNormal() | `vec3 GetNormal()` [VS/GS]<br>`vec3 GetNormal(vec3)` [FS参考] | VS: 用法线矩阵变换；GS/FS: 返回插值值 |
| TexCoord | 开发者直接访问 | N/A | 不需要辅助函数，简单插值 |
| Tangent | GetTangent() | `vec3 GetTangent()` [如果有] | PBR 材质可能需要 |

### 材质实例 (MI) 的自动生成

| Shader Stage | 获取方式 | 自动生成函数 |
|-------------|---------|----------|
| VS | 从顶点属性 MaterialInstanceID 读 SSBO | `MaterialInstance GetMaterialInstance()` |
| GS | 从 Input[i].MaterialInstanceID 读 SSBO | `MaterialInstance GetMaterialInstance(int i)` |
| FS | 从 Input.MaterialInstanceID（插值）读 SSBO | `MaterialInstance GetMaterialInstance()` |

---

## 实现策略：编码决策

### 算法 GenHelperFunctionLibrary()

```cpp
AnsiString GenHelperFunctionLibrary(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    AnsiString result;

    // 1. 所有 stage 都需要这些
    result += GenGetLocalToWorld(def);        // mat4 GetLocalToWorld()
    result += GenGetNormalMatrix(def);        // mat3 GetNormalMatrix()

    // 2. 特定 stage 的辅助函数
    if (strcmp(shader_stage, "VS") == 0) {
        if (HasVertexAttribute(def, VAType::F32_3, "Normal")) {
            result += R"(
vec3 GetNormal() {
    return normalize(GetNormalMatrix() * Normal);
}
)";
        }
        result += R"(
vec3 GetNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}
)";
        result += GenGetPositionFunctions(def, "VS");  // GetPosition3D(), GetClipPosition()
        result += GenGetMaterialInstanceFunctions(def, "VS");
    }
    else if (strcmp(shader_stage, "GS") == 0) {
        // GS 中的函数需要 vertex_index 参数
        result += R"(
vec3 GetWorldNormal(int i) {
    return Input[i].WorldNormal;
}
)";
        result += GenGetMaterialInstanceFunctions(def, "GS");
        result += R"(
void HandoverMaterialInstanceID(int i) {
    Output.MaterialInstanceID = Input[i].MaterialInstanceID;
}
)";
    }
    else if (strcmp(shader_stage, "FS") == 0) {
        // FS 无法访问顶点属性，只有插值值
        result += R"(
vec4 GetPosition3D() {
    return vec4(Input.WorldPosition, 1.0);
}

vec3 GetWorldNormal() {
    return Input.WorldNormal;
}

vec3 GetNormal() {
    return GetWorldNormal();
}
)";
        result += GenGetMaterialInstanceFunctions(def, "FS");
    }

    return result;
}

// 辅助：检查顶点输入中是否有特定属性
static bool HasVertexAttribute(
    const ComposedMaterialDef &def,
    VAType type,
    const char *name)
{
    for (uint32_t i = 0; i < def.vertex_entry_count; i++) {
        if (def.vertex_entries[i].type == type &&
            strcmp(def.vertex_entries[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}
```

---

## 效果对比

### 旧方式（手工选择）
```glsl
// 开发者必须从 MFGetPosition.h 中选择正确的版本
// 需要知道自己用的是什么输入、什么坐标系、什么 stage

#include "MFGetPosition.h"  // 包含 8 种变体
#include "MFCommon.h"        // 包含多个 GetMI 版本
#include "MFGetNormal.h"      // 包含多个 GetNormal 版本

vec4 VertexShaderBusiness(const VertexInput vi) {
    // 不确定该用哪个？GetPosition3DL2WCamera 还是 GetPosition3DL2W?
    vec4 world_pos = GetPosition3DL2WCamera * vec4(vi.Position, 1.0);  // 错了？
    vec3 world_normal = GetNormal(mat3(...), vi.Normal);  // 还要传矩阵？
    MaterialInstance mi = GetMI();  // 这个对吗？在 VS 中？
    return world_pos;
}
```

### 新方式（框架自动）
```glsl
// 框架根据 ComposedMaterialDef 自动生成正确的函数
// 开发者无需关心实现细节，直接调用统一接口

vec4 VertexShaderBusiness(const VertexInput vi) {
    // 框架自动生成的函数库，确保正确
    vec4 world_pos = GetLocalToWorld() * vec4(vi.Position, 1.0);
    vec3 world_normal = GetNormal(vi.Normal);  // 自动使用正确的矩阵
    MaterialInstance mi = GetMaterialInstance();  // 框架自动选择 VS 版本
    return GetClipPosition();  // 自动投影，无需手工计算
}
```

---

## 实现优先级

### Priority 1（立即）
- [ ] `GenHelperFunctionLibrary()` 框架实现
- [ ] `GenGetLocalToWorld()` 针对 Assign 顶点输入
- [ ] `GenGetNormalMatrix()` 基本版本
- [ ] `GenGetMaterialInstanceFunctions()` 针对 SSBO 读取

### Priority 2（M2）
- [ ] 针对不同 shader stage 的版本（VS/GS/FS）
- [ ] 条件生成（有 Normal 才生成 GetNormal()）
- [ ] 坐标系参数化（NDC / ZeroToOne / Ortho）

### Priority 3（M3+）
- [ ] 更复杂的矩阵推导（Tangent space、双法线等）
- [ ] Compute shader 支持
- [ ] 自定义辅助函数挂接

---

## 关键工程决策

### 1. 统一接口原则
无论 VS/GS/FS，开发者始终调用同名函数：
```glsl
GetPosition3D()
GetNormal()
GetMaterialInstance()
```

框架自动生成 stage-specific 实现。

### 2. 条件编译原则
如果材质不需要某个功能（如无 Normal 输入），框架就不生成对应函数。
开发者尝试调用不存在的函数 → 编译错误 → 立刻知道需要添加顶点属性。

### 3. 零运行时成本
所有函数都是 `inline`，最终编译出来的 SPV 与手写等效。

---

## 示例：BasicLit 完整生成流程

### 输入
```cpp
constexpr ComposedMaterialDef BASIC_LIT_COMPOSED {
    .vertex_entries = {
        {VAType::F32_3, "Position"},
        {VAType::F32_3, "Normal"},
        {VAType::F32_2, "TexCoord"},
    },
    .vertex_entry_count = 3,
    .descriptor_entries = {
        {DescriptorKind::UBO, 0, "ViewportInfo"},
        {DescriptorKind::UBO, 1, "CameraInfo"},
        {DescriptorKind::UBO, 2, "LocalToWorld"},
        {DescriptorKind::SSBO, 4, "MaterialInstanceData"},
    },
    // ...
};
```

### 框架调用
```cpp
AnsiString vs_helpers = GenHelperFunctionLibrary(BASIC_LIT_COMPOSED, "VS");
```

### 框架生成的函数库
```glsl
// 坐标变换
mat4 GetLocalToWorld() {
    return l2w.mats[TransformID];
}

mat3 GetNormalMatrix() {
    return mat3(camera.view * GetLocalToWorld());
}

vec4 GetPosition3D() {
    return GetLocalToWorld() * vec4(Position, 1.0);
}

vec4 GetClipPosition() {
    return camera.vp * GetPosition3D();
}

// 法线
vec3 GetNormal() {
    return normalize(GetNormalMatrix() * Normal);
}

vec3 GetNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}

// 材质实例
MaterialInstance GetMaterialInstance() {
    return mtl.mi[MaterialInstanceID];
}

MaterialInstance GetMI() {
    return GetMaterialInstance();
}
```

### 开发者业务代码
```glsl
vec4 VertexShaderBusiness(const VertexInput vi) {
    return GetClipPosition();  // 框架已生成，可直接使用
}
```

---

## 总结

通过**自动生成辅助函数库**，我们实现了：

✅ **开发者零认知成本** —— 不用记住 MFGetPosition.h 有多少种变体  
✅ **类型安全** —— 编译错误立刻告诉开发者需要什么顶点属性  
✅ **零运行时开销** —— inline 函数，编译后与手写等效  
✅ **高度参数化** —— 框架根据 def 和 stage 自动适配  
✅ **易维护** —— 修改坐标变换逻辑一次，所有材质同步更新
