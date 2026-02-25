# ResourceLayoutGenerator Binding 分配策略文档 v1.0

本文档定义了 `ResourceLayoutGenerator` 的 descriptor binding 分配规则、冲突检测机制与调试策略。

**最后更新**：2026-02-26  
**适用范围**：Phase B 及后续所有材质开发  
**关联文档**：
- [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)
- [SHADER_LOGIC_CONSTRAINTS_SPEC.md](SHADER_LOGIC_CONSTRAINTS_SPEC.md)

---

## 1. 核心设计原则

### 1.1 分配策略

✅ **固定映射策略**（当前实现）：
- 材质定义时显式指定每个 descriptor 的 `set` 和 `binding`
- `ResourceLayoutGenerator` **不自动分配**，而是**验证并生成**
- 开发者全权负责避免冲突

```cpp
// 示例：材质定义中显式指定 binding
constexpr MaterialDescriptor EXAMPLE_DESCRIPTORS[] = {
    { .set = 0, .binding = 0, .name = "camera", .type = SET_Uniform, ... },
    { .set = 0, .binding = 1, .name = "l2w", .type = SET_UniformBuffer, ... },
    { .set = 1, .binding = 0, .name = "mtl", .type = SET_StorageBuffer, ... },
};
```

❌ **自动分配策略**（未实现）：
- 开发者只指定 `set`，`binding` 由框架自动分配
- 优点：避免手动管理 binding
- 缺点：可能导致不确定性（不同材质自动分配结果不同）

### 1.2 Descriptor Set 分配约定

| Set 编号 | 用途                              | 更新频率         | 示例                     |
|----------|-----------------------------------|------------------|--------------------------|
| 0        | 全局常量（相机/视口/时间等）      | 每帧 1 次        | camera, viewport         |
| 1        | 每材质常量（变换矩阵/MI Buffer）  | 每批次多次       | l2w, mtl                 |
| 2        | 纹理采样器                        | 每材质实例不同   | diffuseMap, normalMap    |
| 3        | 动态资源（骨骼动画/粒子等）       | 每帧多次         | bones, particleBuffer    |
| 4-7      | 保留（未来扩展：GBuffer/PBR）     | 按需定义         | gBuffer, lightProbes     |

### 1.3 Binding 分配原则

✅ **推荐模式**：
```cpp
// Set 0: 全局常量 (binding 0-5)
{ .set = 0, .binding = 0, .name = "camera", ... },       // 相机信息
{ .set = 0, .binding = 1, .name = "viewport", ... },     // 视口信息
{ .set = 0, .binding = 2, .name = "time", ... },         // 时间信息
{ .set = 0, .binding = 3, .name = "lighting", ... },     // 光照信息

// Set 1: 每材质常量 (binding 0-10)
{ .set = 1, .binding = 0, .name = "l2w", ... },          // LocalToWorld Buffer
{ .set = 1, .binding = 1, .name = "mtl", ... },          // MaterialInstance Buffer

// Set 2: 纹理 (binding 0-15)
{ .set = 2, .binding = 0, .name = "diffuseMap", ... },   // Diffuse 纹理
{ .set = 2, .binding = 1, .name = "normalMap", ... },    // Normal 纹理
{ .set = 2, .binding = 2, .name = "roughnessMap", ... }, // Roughness 纹理
```

❌ **禁止模式**：
```cpp
// ❌ 同一 set 内 binding 跳号（浪费资源）
{ .set = 0, .binding = 0, .name = "camera", ... },
{ .set = 0, .binding = 10, .name = "viewport", ... },  // 跳过 1-9

// ❌ 不同语义混在一个 set（违反更新频率原则）
{ .set = 0, .binding = 0, .name = "camera", ... },     // 全局
{ .set = 0, .binding = 1, .name = "diffuseMap", ... }, // 材质实例（应在 set=2）
```

---

## 2. 冲突检测机制

### 2.1 检测时机

```cpp
// 在生成 layout 代码时检测
AnsiString ResourceLayoutGenerator::GenDescriptorLayout(
    const FixedDescriptorEntry* entries, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        CheckAndMarkBinding(entries[i].set, entries[i].binding, entries[i].name);
        //                  ^^^^^^^^^^^^^ 这里检测重复
    }
    // ...
}
```

### 2.2 检测算法

使用位图（Bitmap）快速检测：
```cpp
struct BindingMap {
    uint32_t sets[8];  // 支持 set=0~7，每个 set 用 32 位表示 binding=0~31
    
    bool IsUsed(uint32_t set, uint32_t binding) const {
        return (sets[set] & (1u << binding)) != 0;
    }
    
    void MarkUsed(uint32_t set, uint32_t binding) {
        sets[set] |= (1u << binding);
    }
};
```

**复杂度**：O(1) 检测，O(n) 遍历所有 descriptors

**限制**：
- 最多支持 8 个 set（0~7）
- 每个 set 最多支持 32 个 binding（0~31）
- 超出范围会输出错误但不崩溃

### 2.3 冲突错误消息

```cpp
void ResourceLayoutGenerator::CheckAndMarkBinding(uint32_t set, uint32_t binding, const char* name)
{
    if (used_bindings.IsUsed(set, binding)) {
        fprintf(stderr, "\n");
        fprintf(stderr, "❌❌❌ DUPLICATE DESCRIPTOR BINDING DETECTED! ❌❌❌\n");
        fprintf(stderr, "   Set=%u, Binding=%u\n", set, binding);
        fprintf(stderr, "   Resource name: %s\n", name);
        fprintf(stderr, "   This is the ROOT CAUSE of shader compilation errors!\n");
        fprintf(stderr, "\n");
        
        #ifdef _DEBUG
            assert(false && "Duplicate descriptor binding!");  // Debug 模式下崩溃
        #endif
    }
    
    used_bindings.MarkUsed(set, binding);
}
```

**示例输出**：
```
❌❌❌ DUPLICATE DESCRIPTOR BINDING DETECTED! ❌❌❌
   Set=1, Binding=0
   Resource name: mtl
   This is the ROOT CAUSE of shader compilation errors!
```

---

## 3. 生成的 GLSL 代码格式

### 3.1 Descriptor 布局

**输入**（C++ descriptor 定义）：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .set = 0, .binding = 0, .name = "camera", .type = SET_Uniform, .stage = STAGE_Vertex },
    { .set = 1, .binding = 0, .name = "mtl", .type = SET_StorageBuffer, .stage = STAGE_Fragment },
};
```

**输出**（GLSL 代码）：
```glsl
// Set 0, Binding 0
layout(set=0, binding=0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 vp;
    vec4 frustum_planes[6];
} camera;

// Set 1, Binding 0
layout(set=1, binding=0) buffer MaterialInstanceBuffer {
    MaterialInstance instances[];
} mtl;
```

### 3.2 Vertex Input 布局

**输入**（C++ 顶点定义）：
```cpp
constexpr FixedVertexEntry VERTEX[] = {
    { .location = 0, .name = "Position", .type = VAType{VertexAttribBaseType::Float, 3} },
    { .location = 1, .name = "Normal", .type = VAType{VertexAttribBaseType::Float, 3} },
};
```

**输出**（GLSL 代码）：
```glsl
layout(location=0) in vec3 Position;
layout(location=1) in vec3 Normal;
```

**注意**：`location` 由材质定义直接指定，不自动分配。

### 3.3 Fragment Output 布局

当前固定输出（未来扩展为 MRT）：
```glsl
layout(location=0) out vec4 FragColor;
```

---

## 4. 常见错误与修复

### 错误 1：重复 binding

❌ **错误代码**：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .set = 0, .binding = 0, .name = "camera", ... },
    { .set = 0, .binding = 0, .name = "viewport", ... },  // ❌ 重复 set=0, binding=0
};
```

✅ **修复**：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .set = 0, .binding = 0, .name = "camera", ... },
    { .set = 0, .binding = 1, .name = "viewport", ... },  // ✅ 改为 binding=1
};
```

### 错误 2：超出 binding 范围

❌ **错误代码**：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .set = 0, .binding = 35, .name = "camera", ... },  // ❌ 超过 binding=31 上限
};
```

**错误输出**：
```
ERROR: Invalid descriptor binding: set=0, binding=35
```

✅ **修复**：
- 将资源分配到不同 set
- 或减少 binding 编号

```cpp
// 方案 1：改用 set=1
{ .set = 1, .binding = 3, .name = "camera", ... },

// 方案 2：减少 binding 编号
{ .set = 0, .binding = 10, .name = "camera", ... },
```

### 错误 3：Set/Binding 不连续

⚠️ **警告代码**（不会报错但不推荐）：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .set = 0, .binding = 0, .name = "camera", ... },
    { .set = 0, .binding = 5, .name = "viewport", ... },  // ⚠️ binding 跳号（浪费 1-4）
};
```

✅ **推荐**：
```cpp
constexpr MaterialDescriptor DESC[] = {
    { .set = 0, .binding = 0, .name = "camera", ... },
    { .set = 0, .binding = 1, .name = "viewport", ... },  // ✅ 连续分配
};
```

---

## 5. Binding 分配最佳实践

### 5.1 按 Set 分组

```cpp
// ═════════════════════════════════════════════════════════════════════════════
// Set 0: 全局常量（所有材质共享）
// ═════════════════════════════════════════════════════════════════════════════
constexpr MaterialDescriptor GLOBAL_DESCRIPTORS[] = {
    { .set = 0, .binding = 0, .name = "camera", .type = SET_Uniform, .stage = STAGE_Vertex },
    { .set = 0, .binding = 1, .name = "viewport", .type = SET_Uniform, .stage = STAGE_Vertex },
    { .set = 0, .binding = 2, .name = "lighting", .type = SET_Uniform, .stage = STAGE_Fragment },
};

// ═════════════════════════════════════════════════════════════════════════════
// Set 1: 材质实例常量
// ═════════════════════════════════════════════════════════════════════════════
constexpr MaterialDescriptor MATERIAL_DESCRIPTORS[] = {
    { .set = 1, .binding = 0, .name = "l2w", .type = SET_UniformBuffer, .stage = STAGE_Vertex },
    { .set = 1, .binding = 1, .name = "mtl", .type = SET_StorageBuffer, .stage = STAGE_Fragment },
};

// ═════════════════════════════════════════════════════════════════════════════
// Set 2: 纹理采样器
// ═════════════════════════════════════════════════════════════════════════════
constexpr MaterialDescriptor TEXTURE_DESCRIPTORS[] = {
    { .set = 2, .binding = 0, .name = "diffuseMap", .type = SET_CombinedImageSampler, .stage = STAGE_Fragment },
    { .set = 2, .binding = 1, .name = "normalMap", .type = SET_CombinedImageSampler, .stage = STAGE_Fragment },
};

// 合并到一个数组（GenDescriptorLayout 使用）
constexpr MaterialDescriptor ALL_DESCRIPTORS[] = {
    GLOBAL_DESCRIPTORS[0], GLOBAL_DESCRIPTORS[1], GLOBAL_DESCRIPTORS[2],
    MATERIAL_DESCRIPTORS[0], MATERIAL_DESCRIPTORS[1],
    TEXTURE_DESCRIPTORS[0], TEXTURE_DESCRIPTORS[1],
};
```

### 5.2 使用宏减少重复

```cpp
// 定义公共 descriptor
#define COMMON_DESC_CAMERA   { .set = 0, .binding = 0, .name = "camera", .type = SET_Uniform, .stage = STAGE_Vertex }
#define COMMON_DESC_VIEWPORT { .set = 0, .binding = 1, .name = "viewport", .type = SET_Uniform, .stage = STAGE_Vertex }
#define COMMON_DESC_L2W      { .set = 1, .binding = 0, .name = "l2w", .type = SET_UniformBuffer, .stage = STAGE_Vertex }
#define COMMON_DESC_MTL      { .set = 1, .binding = 1, .name = "mtl", .type = SET_StorageBuffer, .stage = STAGE_Fragment }

// 在材质中复用
constexpr MaterialDescriptor PURE_COLOR_3D_DESCRIPTORS[] = {
    COMMON_DESC_CAMERA,
    COMMON_DESC_L2W,
    COMMON_DESC_MTL,
};

constexpr MaterialDescriptor VERTEX_COLOR_3D_DESCRIPTORS[] = {
    COMMON_DESC_CAMERA,
    COMMON_DESC_L2W,
    COMMON_DESC_MTL,
};
```

### 5.3 文档化 Binding 分配表

在材质定义文件中维护一个注释表格：

```cpp
/**
 * Descriptor Binding 分配表（PureColor3D）
 * 
 * Set 0: 全局常量
 *   binding=0: camera   (Uniform)
 *   binding=1: viewport (Uniform)
 * 
 * Set 1: 材质常量
 *   binding=0: l2w (UniformBuffer - Transform Matrix)
 *   binding=1: mtl (StorageBuffer - MaterialInstance Data)
 * 
 * Set 2: 纹理
 *   (无)
 */
constexpr MaterialDescriptor PURE_COLOR_3D_DESCRIPTORS[] = {
    /* Set 0 */ { .set = 0, .binding = 0, .name = "camera", ... },
    /* Set 0 */ { .set = 0, .binding = 1, .name = "viewport", ... },
    /* Set 1 */ { .set = 1, .binding = 0, .name = "l2w", ... },
    /* Set 1 */ { .set = 1, .binding = 1, .name = "mtl", ... },
};
```

---

## 6. 未来扩展方向（Phase C+）

### 6.1 自动分配模式（可选）

允许材质只指定 `set`，`binding` 由框架自动分配：

```cpp
// 当前：手动指定 set + binding
{ .set = 1, .binding = 0, .name = "l2w", ... },
{ .set = 1, .binding = 1, .name = "mtl", ... },

// 未来：只指定 set，binding 自动分配
{ .set = 1, .binding = AUTO, .name = "l2w", ... },  // 框架自动分配 binding=0
{ .set = 1, .binding = AUTO, .name = "mtl", ... },  // 框架自动分配 binding=1
```

**实现思路**：
```cpp
void AutoAssignBindings(MaterialDescriptor* descriptors, uint32_t count) {
    std::map<uint32_t, uint32_t> next_binding;  // set → next_binding
    
    for (uint32_t i = 0; i < count; ++i) {
        if (descriptors[i].binding == AUTO) {
            uint32_t set = descriptors[i].set;
            descriptors[i].binding = next_binding[set]++;
        }
    }
}
```

### 6.2 多渲染目标（MRT）支持

当前 Fragment Output 固定为：
```glsl
layout(location=0) out vec4 FragColor;
```

未来扩展为 GBuffer / Deferred Rendering：
```glsl
layout(location=0) out vec4 GBuffer_Albedo;
layout(location=1) out vec4 GBuffer_Normal;
layout(location=2) out vec4 GBuffer_MetallicRoughness;
layout(location=3) out vec4 GBuffer_Emission;
```

### 6.3 Descriptor Indexing（Bindless）

支持 Vulkan 1.2 的 Descriptor Indexing：
```glsl
layout(set=2, binding=0) uniform sampler2D textures[];  // 数组大小不固定

vec4 color = texture(textures[textureIndex], uv);  // 动态索引
```

---

## 7. Phase B 验收标准

### 7.1 文档完整性

- [x] 分配策略明确（固定映射 vs 自动分配）
- [x] 冲突检测机制文档化
- [x] 错误消息格式规范化
- [ ] 在 `ResourceLayoutGenerator.h` 头部增加策略说明注释

### 7.2 代码一致性检查

- [ ] 所有已迁移材质（PureColor / VertexColor）binding 不冲突
- [ ] 没有超出范围的 set/binding（set >= 8 || binding >= 32）
- [ ] Descriptor 按 set 分组，binding 连续分配

### 7.3 测试覆盖

创建故意冲突的测试用例：
```cpp
// test/test_duplicate_binding.cpp
constexpr MaterialDescriptor TEST_DUP[] = {
    { .set = 0, .binding = 0, .name = "res1", ... },
    { .set = 0, .binding = 0, .name = "res2", ... },  // 故意冲突
};

// 预期输出：
// ❌❌❌ DUPLICATE DESCRIPTOR BINDING DETECTED! ❌❌❌
//    Set=0, Binding=0
//    Resource name: res2
```

---

## 8. 调试技巧

### 8.1 启用详细日志

```cpp
// ResourceLayoutGenerator.cpp
#define VERBOSE_BINDING_LOG 1

#ifdef VERBOSE_BINDING_LOG
    fprintf(stderr, "[BindingAlloc] Set=%u, Binding=%u, Name=%s\n", 
            set, binding, name);
#endif
```

### 8.2 可视化 Binding 分配

```cpp
void ResourceLayoutGenerator::PrintBindingMap() const {
    for (uint32_t set = 0; set < 8; ++set) {
        if (used_bindings.sets[set] == 0) continue;
        
        printf("Set %u: ", set);
        for (uint32_t binding = 0; binding < 32; ++binding) {
            if (used_bindings.IsUsed(set, binding)) {
                printf("%u ", binding);
            }
        }
        printf("\n");
    }
}

// 输出示例：
// Set 0: 0 1 2
// Set 1: 0 1
// Set 2: 0 1 2 3
```

### 8.3 运行时断言

```cpp
// Debug 模式下崩溃以便立即发现冲突
#ifdef _DEBUG
    assert(!used_bindings.IsUsed(set, binding) && "Duplicate binding!");
#endif
```

---

## 9. 变更历史

| 版本 | 日期       | 变更内容                                           |
|------|------------|----------------------------------------------------|
| 1.0  | 2026-02-26 | 初版发布，规范固定映射策略 + 冲突检测机制        |

---

**责任人**：Shader System 维护团队  
**审核周期**：Phase B 完成后冻结  
**相关文档**：
- [SHADER_RESOURCE_NAMING_SPEC.md](SHADER_RESOURCE_NAMING_SPEC.md)
- [SHADER_LOGIC_CONSTRAINTS_SPEC.md](SHADER_LOGIC_CONSTRAINTS_SPEC.md)
