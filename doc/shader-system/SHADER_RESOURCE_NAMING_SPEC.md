# Shader Resource 命名与引用规范 v1.0

本文档定义了 ULRE Shader System 中 `required_resources` 的命名规则与引用约定。

**最后更新**：2026-02-28（文档口径统一，无规范变更）  
**适用范围**：Phase B 及后续所有材质开发  
**关联文档**：[SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md)

---

## 1. 核心设计原则

1. **名称即语义**：资源名称清晰表达用途，不使用缩写（如 `cam` → `camera`）
2. **Descriptor 优先**：`required_resources` 中的名称必须与 `FixedMaterialDef.descriptors[].name` 完全一致
3. **大小写敏感**：`CameraInfo` ≠ `camerainfo`，统一使用 PascalCase（首字母大写）
4. **禁止冗余**：不允许同时存在 `camera` 和 `Camera` 或 `CameraInfo` 和 `CameraData`

---

## 2. Descriptor 命名规范

### 2.1 标准命名模式

✅ **推荐命名**（按类别）：

```cpp
// 相机相关
"camera"           // CameraInfo 结构体（包含 view/projection/frustum）
"viewport"         // ViewportInfo 结构体（ortho_matrix/screen_size 等）

// 变换相关
"l2w"              // LocalToWorld Transform Buffer（名称简洁但明确）
"bones"            // Skeleton Joints Buffer（骨骼动画）

// 材质实例
"mtl"              // MaterialInstanceData Buffer（Material 缩写为 mtl）
"mi_set"           // MaterialInstance Array（备用名称，如果 mtl 冲突）

// 纹理采样器
"diffuseMap"       // Diffuse 纹理（PascalCase + Map 后缀）
"normalMap"        // Normal 纹理
"shadowMap"        // Shadow 纹理"envCubemap"       // Environment Cubemap（区分 Map 和 Cubemap）

// 光照相关
"lighting"         // LightingInfo 结构体（环境光/方向光参数）
"lightProbes"      // Light Probe Array（GI 探针）
```

❌ **禁止命名**：
```cpp
"cam"              // 使用 "camera"
"Camera"           // Descriptor 使用小写开头的 camelCase
"CAMERA"           // 不使用全大写
"camera_info"      // 不使用下划线，使用 camelCase
"tex"              // 使用 "diffuseMap" 等明确名称
"mtl_data"         // 使用 "mtl"（后缀 _data 冗余）
```

### 2.2 命名一致性检查表

在定义材质时，确保以下三处名称完全一致：

```cpp
// 1. FixedMaterialDef 中的 descriptor 定义
constexpr MaterialDescriptor EXAMPLE_DESCRIPTORS[] = {
    { .name = "camera", .type = SET_Uniform, .stage = STAGE_Vertex }
    //       ^^^^^^^^ 名称必须与下面两处一致
};

// 2. required_resources 数组
constexpr const char* EXAMPLE_VERTEX_RESOURCES[] = {
    "camera"  // 与 descriptor.name 完全一致
};

// 3. GLSL 代码中的 layout block 名称
const char* EXAMPLE_GLSL = R"(
layout(set=0, binding=0) uniform CameraBlock {
    mat4 view;
    mat4 projection;
} camera;  // 实例名必须与 descriptor.name 一致
//  ^^^^^^
)";
```

**验证规则**：
- MaterialCompiler 会检查 `required_resources` 中的每个名称是否在 `descriptors[]` 中存在
- 如果找不到匹配，编译时报错：`[Error] Required resource 'XXX' not found in material descriptors`

---

## 3. GLSL Uniform Block 命名规范

### 3.1 结构体定义与实例化

✅ **推荐模式**：
```glsl
// 定义结构体（PascalCase）
layout(set=0, binding=0) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    mat4 vp;
    vec4 frustum_planes[6];
} camera;  // 实例名（camelCase，与 descriptor.name 一致）
```

使用时直接引用实例名：
```glsl
gl_Position = camera.vp * worldPos;  // ✅ 正确
```

❌ **错误模式**：
```glsl
// ❌ 实例名与 descriptor 不一致
} cameraData;  // descriptor.name 是 "camera"，这里却用 cameraData

// ❌ 使用结构体类型名（GLSL 会报错）
gl_Position = CameraBlock.vp * worldPos;  // 编译错误
```

### 3.2 SSBO 命名规范

```glsl
// Storage Buffer（动态数组）
layout(set=1, binding=0) buffer MaterialInstanceBuffer {
    MaterialInstance instances[];
} mtl;  // 实例名必须与 descriptor.name 一致

// 使用方式
MaterialInstance mi = mtl.instances[MaterialInstanceID];  // ✅
```

### 3.3 Sampler 命名规范

```glsl
// 纹理采样器（直接绑定，无结构体）
layout(set=2, binding=0) uniform sampler2D diffuseMap;  // 名称即 descriptor.name

// 使用方式
vec4 color = texture(diffuseMap, uv);  // ✅
```

---

## 4. required_resources 引用规则

### 4.1 明确依赖关系

每个 Shader Stage 只声明它实际使用的资源：

```cpp
// Vertex Shader 需要的资源
constexpr const char* MY_VERTEX_RESOURCES[] = {
    "camera",    // VS 需要 camera.vp 计算 clip position
    "l2w"        // VS 需要 LocalToWorld 变换
};

// Fragment Shader 需要的资源
constexpr const char* MY_FRAGMENT_RESOURCES[] = {
    "mtl",         // FS 需要 MaterialInstance（颜色/参数）
    "diffuseMap"   // FS 需要纹理采样
    // 注意：不需要 "camera"（VS 已传递 clip position）
};
```

**优点**：
- 清晰的依赖关系便于优化
- 编译器可以检测未使用的资源
- 减少无效绑定（如 FS 不需要 camera 却绑定了）

### 4.2 资源缺失诊断

如果业务代码中使用了未声明的资源，编译器会报错：

```cpp
// ❌ 错误示例
constexpr const char MY_VS_LOGIC[] = R"(
vec4 pos = camera.vp * worldPos;  // 使用了 camera
)";

constexpr const char* MY_VERTEX_RESOURCES[] = {
    "l2w"  // 声明了 l2w，但没有声明 camera
};

// 编译时错误：
// [Error] Shader references 'camera' but not in required_resources
```

**修复方式**：
```cpp
constexpr const char* MY_VERTEX_RESOURCES[] = {
    "camera",  // ✅ 增加 camera 声明
    "l2w"
};
```

---

## 5. 特殊资源命名约定

### 5.1 系统保留名称

以下名称由框架自动注入，材质不应重复定义：

```cpp
// ⚠️ 系统保留（不要在 descriptors 中定义）
"viewport"      // 视口信息（全局注入）
"time"          // 全局时间（全局注入）
"globalParams"  // 引擎全局参数（全局注入）
```

如果材质定义了同名 descriptor，会覆盖系统默认值（通常不推荐）。

### 5.2 约定俗成的简称

| 完整名称           | 约定简称 | 用途                      |
|--------------------|----------|---------------------------|
| LocalToWorld       | `l2w`    | Transform Buffer          |
| MaterialInstance   | `mtl`    | Material Instance Buffer  |
| CameraInfo         | `camera` | Camera Uniform Block      |
| LightingInfo       | `lighting` | Lighting Parameters     |

**注意**：简称必须在整个代码库中保持一致，不允许同时存在 `l2w` 和 `localToWorld`。

---

## 6. 常见错误与修复

### 错误 1：名称不匹配

❌ **错误代码**：
```cpp
// descriptor 定义
constexpr MaterialDescriptor DESC[] = {
    { .name = "camera", ... }
};

// required_resources 引用错误
constexpr const char* RESOURCES[] = {
    "Camera"  // 大小写不匹配
};
```

✅ **修复**：
```cpp
constexpr const char* RESOURCES[] = {
    "camera"  // 与 descriptor.name 完全一致
};
```

### 错误 2：GLSL 实例名不一致

❌误代码**：
```cpp
// descriptor.name = "camera"
const char* GLSL = R"(
layout(...) uniform CameraBlock {
    ...
} cam;  // 实例名错误
)";
```

✅ **修复**：
```cpp
const char* GLSL = R"(
layout(...) uniform CameraBlock {
    ...
} camera;  // 与 descriptor.name 一致
)";
```

### 错误 3：重复定义

❌ **错误代码**：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .name = "camera", ... },
    { .name = "cameraInfo", ... }  // 语义重复
};
```

✅ **修复**：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .name = "camera", ... }  // 只保留一个标准名称
};
```

---

## 7. Phase B 验收标准

### 7.1 检查清单

- [ ] 所有 `descriptor.name` 使用 camelCase（首字母小写）
- [ ] 所有 `required_resources` 字符串与对应 `descriptor.name` 完全匹配
- [ ] GLSL 中 uniform block 实例名与 `descriptor.name` 一致
- [ ] 无重复定义（如 `camera` / `cameraInfo` 同时存在）
- [ ] 已迁移材质（PureColor / VertexColor）通过资源验证

### 7.2 自动化检查工具（计划）

Phase C 将增加编译时检查：
```cpp
// 伪代码
for (auto& res_name : required_resources) {
    if (!find_descriptor(descriptors, res_name)) {
        error("Required resource '%s' not found in descriptors", res_name);
    }
}

for (auto& desc : descriptors) {
    if (!is_camelCase(desc.name)) {
        warning("Descriptor name '%s' should use camelCase", desc.name);
    }
}
```

---

## 8. 迁移指南

如果现有材质使用了不规范的命名，按以下步骤修复：

1. **检查 descriptor 定义**：确保所有 `descriptor.name` 使用 camelCase
2. **同步 required_resources**：确保字符串与 descriptor.name 一致
3. **更新 GLSL 代码**：uniform block 实例名与 descriptor.name 一致
4. **删除重复定义**：合并语义相同的 descriptor（如 `camera` 和 `cameraInfo`）
5. **测试编译**：运行材质编译器检查错误

### 迁移示例

修改前：
```cpp
// descriptor
{ .name = "Camera", ... }  // ❌ 大写开头

// required_resources
"cam"  // ❌ 与 descriptor 不一致

// GLSL
} cameraData;  // ❌ 实例名不一致
```

修改后：
```cpp
// descriptor
{ .name = "camera", ... }  // ✅ camelCase

// required_resources
"camera"  // ✅ 完全匹配

// GLSL
} camera;  // ✅ 实例名一致
```

---

## 9. 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0  | 2026-02-26 | 初版发布，规范 descriptor / required_resources / GLSL 命名一致性 |
| 1.1  | 2026-02-28 | 文档口径同步：更新时间与阶段状态说明对齐（无规范变更） |

---

**责任人**：Shader System 维护团队  
**审核周期**：Phase B 完成后冻结，Phase E 总结时最终审查  
**相关文档**：[SHADER_HELPER_FUNCTION_SPEC.md](SHADER_HELPER_FUNCTION_SPEC.md)
