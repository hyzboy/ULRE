# 材质 Shader 生成器重新设计方案

> **前提**：渲染架构已确定为"极致性能 + 少量材质"路线——全场景 2-3 个 DrawCall，
> TransformID / MaterialInstanceID 按 Instance 传递，纹理走 Texture2DArray，UberMaterial
> 路线已实现。材质 Shader 生成器**不必**往 Unreal 材质图那种自由度方向设计。

---

## 一、现有生成器的核心问题

### 1.1 为灵活性设计，不为固定材质设计

`ShaderCreateInfo` 的设计假设：
- 材质数量不定、形式不定
- shader 需要在运行时动态拼接（ProcUBO / ProcSSBO / ProcSampler / ProcInput / ProcOutput …）
- GLSL 代码片段散落在 `MFCommon.h`、`MFGetPosition.h`、`MFGetNormal.h` 等多个头文件的 `constexpr const char*` 中

实际现状：
- 整个仓库的内置材质数量是固定且少量的（BasicLit、VertexColor3D、Gizmo3D、Billboard2D 等约 15 个）
- 结构从不改变——每种材质的 descriptor set layout、vertex input、shader stages 在**编译期**就完全确定

动态拼接带来的额外开销：
- 每次创建材质时：字符串拼接 + glslang 编译 + SPV 反射
- `MaterialDescriptorInfo::Resort()` 用 `std::vector` 遍历构建 set/binding 号
- `ShaderCreateInfo::CreateShader()` 里 498 行代码全部是字符串操作

### 1.2 `#ifdef` 二态布局问题

`MaterialCreateInfo` 的私有成员随 `HGL_L2W_USE_SSBO` / `HGL_MI_USE_SSBO` 编译宏变化，
导致同一个对象两种编译模式下内存布局不同。

### 1.3 `DescriptorSetType` 枚举有 9 个槽，实际使用稀疏

材质实际用到的 set 类型：
- 2D 材质：RenderTarget + PerMaterial
- 3D 材质：RenderTarget + Camera + PerFrame + PerMaterial

World / Static / Global / Instance 要么未用，要么可以合并。

### 1.4 `ShaderDescriptorSet::count` 和 `set` 字段依赖运行时副作用

`AddDescriptor()` 每次调用才递增 `count`；`Resort()` 之前 `set` 字段为 -1，
之后才有效——整个过程没有任何静态保证。

---

## 二、新设计目标

| 目标 | 具体含义 |
|------|---------|
| **编译期确定所有材质布局** | descriptor set layout、vertex input、shader stages 全部在 `.cpp` 中写死 |
| **无运行时 GLSL 拼接** | shader 以完整 GLSL 字符串（或外部 `.glsl` 文件）提供，`ShaderCreateInfo` 仅负责编译到 SPV |
| **无运行时字符串查找** | binding 号在材质创建时一次性缓存，渲染时直接用索引访问 |
| **消除 `#ifdef` 二态布局** | UBO/SSBO 后端由运行时枚举决定，不影响对象布局 |
| **保持向后兼容** | 现有 15 个内置材质的 `Create*` 工厂函数接口不变 |

---

## 三、新设计：`FixedMaterial` 取代 `StdMaterial + ShaderCreateInfo`

### 3.1 核心思路

把"shader 生成"拆成两个完全独立的层：

```
Layer A: FixedMaterialDef（编译期常量）
  ─ 完整的 GLSL vertex + fragment 字符串（或 #embed 嵌入的 .glsl）
  ─ 描述符布局表（constexpr 数组）
  ─ 顶点输入表（constexpr 数组）

Layer B: MaterialCompiler（运行时，仅做 glslang 编译）
  ─ 输入：FixedMaterialDef + VulkanDevAttr
  ─ 输出：MaterialCreateInfo（仅含 SPV + 描述符布局）
  ─ 不做任何字符串拼接，只编译
```

与现有代码的对应关系：

```
旧: StdMaterial → ShaderCreateInfo → 动态拼接 GLSL → 编译
新: FixedMaterialDef（constexpr）→ MaterialCompiler → 编译（仅此一步）
```

### 3.2 `FixedDescriptorEntry`（编译期描述符表）

```cpp
/// inc/hgl/graph/mtl/FixedMaterialDef.h

enum class DescriptorKind : uint8 { UBO, SSBO, Texture, TextureSampler };

struct FixedDescriptorEntry
{
    DescriptorSetType   set_type;
    DescriptorKind      kind;
    uint32_t            stage_flags;    // VkShaderStageFlagBits 组合
    const char *        name;           // 绑定名称（binding resolution 用）
    const char *        struct_name;    // GLSL 结构体名称（UBO/SSBO 用）
    const char *        glsl_type;      // sampler2D / sampler2DArray 等（Texture 用）
};
```

使用示例（BasicLit 材质）：

```cpp
constexpr FixedDescriptorEntry BASIC_LIT_DESCRIPTORS[] = {
    { DescriptorSetType::RenderTarget, DescriptorKind::UBO,
      VK_SHADER_STAGE_ALL_GRAPHICS, "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Camera, DescriptorKind::SSBO,
      VK_SHADER_STAGE_ALL_GRAPHICS, "camera", "CameraInfo", nullptr },
    { DescriptorSetType::Camera, DescriptorKind::SSBO,
      VK_SHADER_STAGE_FRAGMENT_BIT, "sky", "SkyInfo", nullptr },
    { DescriptorSetType::PerFrame, DescriptorKind::SSBO,
      VK_SHADER_STAGE_VERTEX_BIT, "l2w", "LocalToWorldData", nullptr },
    { DescriptorSetType::PerMaterial, DescriptorKind::SSBO,
      VK_SHADER_STAGE_FRAGMENT_BIT, "mtl", "MaterialInstanceData", nullptr },
};
```

### 3.3 `FixedVertexEntry`（编译期顶点输入表）

```cpp
struct FixedVertexEntry
{
    VAType              type;
    VertexInputGroup    group;
    VkVertexInputRate   input_rate;
    const char *        name;
};

constexpr FixedVertexEntry BASIC_LIT_VERTEX[] = {
    { VAT_VEC3,  VertexInputGroup::Basic,               VK_VERTEX_INPUT_RATE_VERTEX,   VAN::Position },
    { VAT_VEC3,  VertexInputGroup::Basic,               VK_VERTEX_INPUT_RATE_VERTEX,   VAN::Normal },
    { VAT_UINT,  VertexInputGroup::TransformID,         VK_VERTEX_INPUT_RATE_INSTANCE, VAN::TransformID },
    { VAT_UINT,  VertexInputGroup::MaterialInstanceID,  VK_VERTEX_INPUT_RATE_INSTANCE, VAN::MaterialInstanceID },
};
```

### 3.4 `FixedMaterialDef`（完整材质定义，全部 constexpr）

```cpp
struct FixedMaterialDef
{
    const char *                    name;

    PrimitiveType                   primitive_type;

    // 顶点输入
    const FixedVertexEntry *        vertex_entries;
    uint32_t                        vertex_entry_count;

    // 描述符
    const FixedDescriptorEntry *    descriptor_entries;
    uint32_t                        descriptor_entry_count;

    // MaterialInstance 数据
    const char *                    mi_glsl_codes;      // nullptr = 无
    uint32_t                        mi_struct_bytes;

    // GLSL 源码（完整文件，不是片段）
    const char *                    vert_glsl;
    const char *                    geom_glsl;          // nullptr = 无几何 shader
    const char *                    frag_glsl;
};
```

### 3.5 `MaterialCompiler`（唯一的运行时工作）

```cpp
/// src/ShaderGen/MaterialCompiler.cpp

MaterialCreateInfo *CompileFixedMaterial(
    const VulkanDevAttr *dev_attr,
    const FixedMaterialDef &def);
```

内部实现：
1. 按 `def.descriptor_entries` 构建 `MaterialDescriptorInfo`（顺序固定，无需 `Resort()`）
2. 调用 glslang 编译 `def.vert_glsl` / `def.frag_glsl` → SPV
3. 填充 `MaterialCreateInfo` 并返回

整个过程**没有字符串拼接**，只有编译。

---

## 四、对现有代码的修改量评估

### 4.1 `ShaderCreateInfo` / `ShaderCreateInfoVertex/Fragment/Geometry`

**现状**：498 + 183 + 72 = 753 行，用于动态拼接 GLSL。

**新设计中的角色**：成为 `MaterialCompiler` 的内部实现细节，对外只暴露：
```cpp
bool CompileGLSL(VkShaderStageFlagBits stage, const char *glsl_src, SPVData *&out);
```

`ProcUBO` / `ProcSSBO` / `ProcInput` / `ProcOutput` 等方法不再是公共 API，
最终可以全部删除（分步迁移：先保留，最后一次性删）。

### 4.2 `MaterialDescriptorInfo`

**现状**：150 行，用 `UnorderedMap` 按名称查找描述符，`Resort()` 动态分配 set/binding 号。

**新设计中的角色**：
- 仅在 `CompileFixedMaterial` 中被使用一次（材质创建时）
- 输入是 `FixedDescriptorEntry[]`（有序、无重复），`Resort()` 退化为简单的 for 循环赋值
- 4 个 `UnorderedMap` 可以删除（现在用于名称查找重复项，新设计中数组本身就保证唯一性）

### 4.3 现有 15 个 `M_Xxx.cpp` 文件

每个文件的结构：
```
class MaterialXxx : public Std3DMaterial { ... };    ← 约 30-60 行 GLSL 片段拼接逻辑
MaterialCreateInfo *CreateXxx(dev_attr, cfg) {
    MaterialXxx m(cfg);
    return m.Create(dev_attr);
}
```

迁移路径（不破坏现有接口）：
1. 在每个 `M_Xxx.cpp` 旁新建 `S_Xxx.h`：定义 `constexpr FixedMaterialDef MATERIAL_XXX_DEF`
2. 修改工厂函数为：`return CompileFixedMaterial(dev_attr, MATERIAL_XXX_DEF);`
3. 删除 `class MaterialXxx : public Std3DMaterial`
4. 最终，当所有材质迁移完成后，删除 `Std2DMaterial` / `Std3DMaterial` / `ShaderCreateInfo` / `MaterialDescriptorInfo`

### 4.4 `#ifdef` 二态布局消除

在 `FixedDescriptorEntry` 中直接使用 `DescriptorKind::SSBO` 或 `DescriptorKind::UBO`，
无需 `#ifdef`：

```cpp
// RenderOptions.h 中的宏只用于选择 FixedDescriptorEntry 中的 kind 字段值
constexpr DescriptorKind L2W_BUFFER_KIND =
    #if HGL_L2W_USE_SSBO
        DescriptorKind::SSBO
    #else
        DescriptorKind::UBO
    #endif
    ;
```

`MaterialCreateInfo` 自身的成员布局不再随宏变化。

---

## 五、`DescriptorSetType` 枚举精简

当前 9 个槽位，实际使用的不超过 4 个。建议精简为：

```cpp
enum class DescriptorSetType
{
    RenderTarget,   // set=0: Viewport、HDR 显示参数
    Camera,         // set=1: 相机、天空、环境
    PerFrame,       // set=2: L2W 矩阵、骨骼、动态光源（每帧更新的大 SSBO）
    PerMaterial,    // set=3: 材质实例数据 + 纹理（因材质而异）

    ENUM_CLASS_RANGE(RenderTarget, PerMaterial)
};
```

原 `World` / `Static` / `Global` / `Instance` 的数据按语义分别归入 `Camera` 或 `PerFrame`。

减少枚举槽位的直接收益：
- `Material::mp_array[]` 从 9 个缩短为 4 个
- `PipelineLayoutData::layouts[]` 从 9 个缩短为 4 个
- `BindDescriptorSets()` 遍历次数减少

---

## 六、分阶段实施路线

| 阶段 | 内容 | 影响范围 | 工作量 |
|------|------|---------|--------|
| P0 | 移除 `DescriptorSet::BindTexture/BindTextureSampler` 中的调试 `LogInfo`（渲染循环热路径） | 2 个函数 | 极小 |
| P1 | 新增 `FixedMaterialDef` + `FixedDescriptorEntry` + `FixedVertexEntry` 结构体头文件 | 仅新增（已完成）| 小 |
| P2 | 实现 `CompileFixedMaterial()`（内部调用现有 `MaterialDescriptorInfo + Resort + ShaderCreateInfo::CompileToSPV`） | 新增 1 个 .cpp | 中 |
| P3 | 逐个将 `M_Xxx.cpp` 迁移到 `FixedMaterialDef` 模式（接口不变） | 每个 M_Xxx.cpp 独立 | 中（可分批） |
| P4 | `DescriptorSetType` 枚举精简为 4 个 | 全局替换，需测试 | 大 |
| P5 | 删除 `Std2DMaterial` / `Std3DMaterial` / `ShaderCreateInfo` / `MaterialDescriptorInfo` | 清理 | 小（P3 完成后） |

P0 可以立即执行，P1-P3 互不依赖、可以并行开展，P4 需谨慎回归测试。

---

## 七、与 UberMaterial 架构的对应关系

用户已经实现的 UberMaterial 架构在本设计中完全适配：

```
UberMaterial（实现者自定义）
  → 在 S_UberMaterial3D.h 中定义 constexpr FixedMaterialDef UBER_MATERIAL_DEF
  → 调用 CompileFixedMaterial(dev_attr, UBER_MATERIAL_DEF)
  → 返回 MaterialCreateInfo（含 SPV + 4 个 DescriptorSet 布局）
  → MaterialManager::CreateMaterial() 的其余流程完全不变
```

UberMaterial 的 `FixedDescriptorEntry[]` 会非常简单（只有 4 个 slot，且全部固定），
完全不需要 `Std3DMaterial::CustomVertexShader()` 这类动态装配逻辑。
