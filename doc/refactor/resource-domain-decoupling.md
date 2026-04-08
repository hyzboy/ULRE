# MaterialResourceDomain 解耦设计

> 2026-04-08 · 基于 Phase 2c 完成后的架构分析

---

## 一、现状问题

### 1.1 当前耦合关系

```
MaterialCreateInfo (shader 编译阶段)
    material_instance_stride  → 从 GLSL struct MaterialInstance {} 反射得出
                ↓
MaterialFinalizeFlowAdapter
    out_plan.mi_data_bytes = mci.GetMaterialInstanceStride()
                ↓
MaterialTemplate
    mi_data_bytes   ← 存储在 Template 上
    mi_max_count    ← 存储在 Template 上
                ↓
MaterialResourceDomain(MaterialTemplate *mtl)  ← 从 Template 读取 stride, max_count
                ↓
DomainMaterialBinding
    校验: domain->GetMIDataBytes() == material->GetMIDataBytes()  ← 字节级碰巧匹配
```

### 1.2 问题清单

| # | 问题 | 后果 |
|---|------|------|
| **D1** | Domain 的数据格式由 MaterialTemplate 决定 | Domain 不能独立于 Material 存在。换 Material 时如果新 Material 碰巧 stride 不同，Domain 废弃 |
| **D2** | 校验只比 `mi_data_bytes`（字节长度） | 两种不同语义的布局可能 stride 相同（如 `Color4f` = 16B 与假设未来的 `UVOffset4` = 16B），通过校验但数据完全错误 |
| **D3** | `texture_array_slot_flags` 放在 MaterialTemplate 上 | Material 声明 "我提供 TextureArray"——实际上资源由 Domain 提供，Material 只是使用方 |
| **D4** | Domain 构造函数依赖 `MaterialTemplate*` | 语义上 Domain 是资源池（调色板），跟画笔（Material）无关 |

### 1.3 真实需求

- **Domain 是资源提供方**：管理实例数据池（Color4f 等）+ TextureArray 资源
- **MaterialTemplate 是资源消费方**：声明需要什么格式的实例数据 + 需要哪些 TextureArray slot
- **DomainMaterialBinding 是桥接层**：校验供需匹配，绑定描述符

类比：

```
Domain     = 调色板（固定的颜色数据结构，跟画法无关）
Material   = 画笔（油画/水彩/素描，每种需要不同的颜色信息子集）
Primitive  = 画布上的一笔（选了一个调色板槽位 + 一支画笔）
```

---

## 二、设计方案

### 2.1 核心思想：InstanceDataLayout 枚举作为数据格式契约

引入 `enum class InstanceDataLayout` 作为 Domain 和 Material 之间的**语义级契约**，替代当前的 byte-stride 隐式匹配。

```
                InstanceDataLayout (enum class)
                /                  \
     Domain 按 enum 创建          Material 按 enum 声明需求
     (提供方)                     (消费方)
                \                  /
          DomainMaterialBinding 校验 enum 一致
```

### 2.2 InstanceDataLayout 定义

```cpp
// inc/hgl/mtl/InstanceDataLayout.h

enum class InstanceDataLayout : uint8_t
{
    None = 0,           ///< 无实例数据 (stride = 0)
    Color4f,            ///< vec4 color — Gizmo3D, Unlit 着色
    PBRColor,           ///< uint base_color + float metallic + float roughness — PBR 打包色
    PBRStandard,        ///< uint base_color + float metallic + float roughness + float normal_scale — PBR 标准
    TextureBlinnPhong,  ///< float normal_strength — 纹理 Blinn-Phong
    // 未来按需扩展...

    ENUM_CLASS_RANGE(None, TextureBlinnPhong)
};
```

### 2.3 布局注册表（constexpr 查表）

```cpp
// inc/hgl/mtl/InstanceDataLayout.h (续)

struct InstanceDataLayoutInfo
{
    uint32_t    stride;         ///< 单个实例数据字节数
    const char *name;           ///< 调试名
    const char *glsl_struct;    ///< 对应 GLSL struct 名（用于校验/文档）
};

constexpr InstanceDataLayoutInfo kInstanceDataLayouts[] =
{
    /* None              */ { 0,  "None",              nullptr },
    /* Color4f           */ { 16, "Color4f",           "MaterialInstance_Color4f" },
    /* PBRColor          */ { 12, "PBRColor",          "MaterialInstance_PBRColor" },
    /* PBRStandard       */ { 16, "PBRStandard",       "MaterialInstance_PBRStandard" },
    /* TextureBlinnPhong */ { 4,  "TextureBlinnPhong", "MaterialInstance_TextureBlinnPhong" },
};

static_assert(sizeof(kInstanceDataLayouts) / sizeof(kInstanceDataLayouts[0])
              == size_t(InstanceDataLayout::RANGE_SIZE),
              "kInstanceDataLayouts must match InstanceDataLayout enum");

inline constexpr uint32_t GetInstanceDataStride(InstanceDataLayout layout)
{
    return kInstanceDataLayouts[uint8_t(layout)].stride;
}

inline constexpr const char *GetInstanceDataName(InstanceDataLayout layout)
{
    return kInstanceDataLayouts[uint8_t(layout)].name;
}
```

### 2.4 GLSL 侧：权威结构体声明

每种布局对应一个独立 `.glsl` 文件，作为 C++ stride 的权威参照源：

```
ShaderLibrary/instance_data/
    Color4f.glsl
    PBRColor.glsl
    PBRStandard.glsl
    TextureBlinnPhong.glsl
```

示例 `Color4f.glsl`：

```glsl
// instance_data/Color4f.glsl
// InstanceDataLayout::Color4f — stride = 16 bytes
struct MaterialInstance
{
    vec4 color;         // 16 bytes
};
```

示例 `PBRStandard.glsl`：

```glsl
// instance_data/PBRStandard.glsl
// InstanceDataLayout::PBRStandard — stride = 16 bytes
struct MaterialInstance
{
    uint  base_color;       // 4 bytes (packed RGBA)
    float metallic;         // 4 bytes
    float roughness;        // 4 bytes
    float normal_scale;     // 4 bytes
};
```

> **注**：各 surface shader（`gizmo3d_surface.glsl` 等）现有的 inline `struct MaterialInstance` 将逐步迁移为 `#include "instance_data/Color4f.glsl"` 等。这属于 shader 层面的整理，可独立于 C++ 改造进行。

### 2.5 TextureArray Slot Flags 归属重新分配

**核心原则：供需分离**

| 角色 | 持有字段 | 语义 |
|------|---------|------|
| **Domain**（供方） | `texture_array_slot_flags` | "我为 BaseColor / Normal / OpacityMask 提供了 TextureArray" |
| **Material**（需方） | `required_texture_array_slots` | "我需要读取 OpacityMask 的 TextureArray"（如 ShadowMap） |

**校验逻辑**：

```cpp
uint8_t required = material->GetRequiredTextureArraySlots();
uint8_t available = domain->GetTextureArraySlots();
if ((required & available) != required)
    // 域未提供材质所需的 TextureArray slot → 拒绝创建 DMB
```

**实际场景**：

```
Domain 注册了 TextureArray:  BaseColor ✓  Normal ✓  OpacityMask ✓

PBR Material         需要:  BaseColor ✓  Normal ✓  OpacityMask ✓  → 校验通过 ✓
ShadowMap Material   需要:                          OpacityMask ✓  → 校验通过 ✓
Unlit Material       需要:  BaseColor ✓                             → 校验通过 ✓
Debug Material       需要:  (无)                                    → 校验通过 ✓
```

**MIT packed 数组长度由 Domain 全集决定**：

```
mit_packed_count = popcount(domain->GetTextureArraySlots())
```

理由：同一 Domain 下不同 Material 变种需要不同 slot 子集。使用供方全集保证**任意变种切换不需要重新分配 MIT 数组**。Material 只读自己需要的 slot，多出来的数据在 SSBO 中存在但不被采样，零额外开销。

---

## 三、各类改造详情

### 3.1 MaterialResourceDomain

```diff
 class MaterialResourceDomain
 {
-    uint32_t  mi_data_bytes     = 0;
-    uint32_t  mi_max_count      = 0;
+    InstanceDataLayout  instance_layout      = InstanceDataLayout::None;
+    uint32_t            mi_max_count         = 0;
+    uint8_t             texture_array_slot_flags = 0;  // 从 MaterialTemplate 迁入

     ActiveMemoryBlockManager *mi_data_manager = nullptr;

-    MaterialResourceDomain(uint32_t mi_bytes, uint32_t mi_count);
-    explicit MaterialResourceDomain(MaterialTemplate *mtl);
+    MaterialResourceDomain(InstanceDataLayout layout, uint32_t max_count,
+                           uint8_t tex_array_slots = 0);

 public:

-    bool     hasMI()          const { return mi_data_bytes > 0; }
-    uint32_t GetMIDataBytes() const { return mi_data_bytes; }
+    InstanceDataLayout GetLayout()   const { return instance_layout; }
+    bool     hasMI()                 const { return instance_layout != InstanceDataLayout::None; }
+    uint32_t GetMIDataBytes()        const { return GetInstanceDataStride(instance_layout); }
+    uint8_t  GetTextureArraySlots()  const { return texture_array_slot_flags; }
     uint32_t GetMIMaxCount()  const { return mi_max_count; }

     int  AllocMISlot();
     void FreeMISlot(int mi_id);
     void *GetMIData(int mi_id);
 };
```

> `GetMIDataBytes()` 保留接口不变，内部改为查表。所有下游消费者不需要修改。

### 3.2 MaterialTemplate

```diff
 class MaterialTemplate
 {
-    uint32_t mi_data_bytes;
-    uint32_t mi_max_count;
-    uint8_t texture_array_slot_flags = 0;
+    InstanceDataLayout required_instance_layout = InstanceDataLayout::None;
+    uint32_t mi_max_count;                      // 渲染批次最大实例数（保留，与 layout 正交）
+    uint8_t  required_texture_array_slots = 0;   // 需方：需要哪些 slot 的 TextureArray

 public:

-    const bool    hasMI           ()const{ return mi_data_bytes > 0; }
-    const uint32_t GetMIDataBytes ()const{ return mi_data_bytes; }
+    InstanceDataLayout GetRequiredLayout()    const { return required_instance_layout; }
+    const bool    hasMI           ()          const { return required_instance_layout != InstanceDataLayout::None; }
+    const uint32_t GetMIDataBytes ()          const { return GetInstanceDataStride(required_instance_layout); }

-    void    SetTextureArraySlotFlags(uint8_t f){ texture_array_slot_flags = f; }
-    uint8_t GetTextureArraySlotFlags()const{ return texture_array_slot_flags; }
+    void    SetRequiredTextureArraySlots(uint8_t f){ required_texture_array_slots = f; }
+    uint8_t GetRequiredTextureArraySlots()const{ return required_texture_array_slots; }
 };
```

> `GetMIDataBytes()` 接口不变 → 下游零改动。
> `GetTextureArraySlotFlags()` 改名为 `GetRequiredTextureArraySlots()` → 调用点需逐一更新，但语义变得正确。

### 3.3 DomainMaterialBinding 校验

```diff
 DomainMaterialBinding *MaterialManager::CreateDomainMaterialBinding(
     MaterialResourceDomain *domain, MaterialTemplate *material)
 {
-    if (domain->GetMIDataBytes() != material->GetMIDataBytes())
-        return nullptr;
+    // 语义级校验 ①：实例数据布局匹配
+    if (domain->GetLayout() != material->GetRequiredLayout())
+    {
+        LOG_ERROR("DMB layout mismatch: domain=%s, material=%s",
+                  GetInstanceDataName(domain->GetLayout()),
+                  GetInstanceDataName(material->GetRequiredLayout()));
+        return nullptr;
+    }
+
+    // 语义级校验 ②：TextureArray slot 供需满足
+    uint8_t required = material->GetRequiredTextureArraySlots();
+    uint8_t available = domain->GetTextureArraySlots();
+    if ((required & available) != required)
+    {
+        LOG_ERROR("DMB texture slot mismatch: required=0x%02X, available=0x%02X",
+                  required, available);
+        return nullptr;
+    }

     // ... 创建 DMB
 }
```

### 3.4 Primitive / PrimitiveMaterialSlot

`PrimitiveMaterialSlot` 字段不变——它携带的是运行时值（`domain*`, `mi_id`, `material_template*`），不关心布局是怎么定义的。

MIT 初始化来源改为 Domain：

```diff
 // Primitive::InitMITLayout 的调用来源
- InitMITLayout(material->GetTextureArraySlotFlags());
+ InitMITLayout(domain->GetTextureArraySlots());
```

理由：MIT packed 数组覆盖 Domain 提供的全集 slot，保证同域内变种切换不需重新分配。

### 3.5 MaterialCreateInfo → InstanceDataLayout 映射

当前 Shader 编译阶段（`MaterialCreateInfo`）通过反射 GLSL `struct MaterialInstance` 得到 `material_instance_stride`。改造后需要增加一步**反向查表**：

```cpp
// MaterialFinalizeFlowAdapter.cpp
InstanceDataLayout ResolveLayoutFromStride(uint32_t stride)
{
    for (uint8_t i = 0; i < uint8_t(InstanceDataLayout::RANGE_SIZE); ++i)
        if (kInstanceDataLayouts[i].stride == stride)
            return InstanceDataLayout(i);
    return InstanceDataLayout::None;  // 未注册的布局
}

void Adapt(const MaterialCreateInfo &mci, FinalizePlan &out_plan)
{
    out_plan.instance_layout = ResolveLayoutFromStride(mci.GetMaterialInstanceStride());
    // ...
}
```

> **注意 stride 碰撞**：目前 `Color4f` (16B) 和 `PBRStandard` (16B) stride 相同。纯靠 stride 无法区分。长期方案是让 shader 编译阶段直接输出 `InstanceDataLayout` 枚举值（通过 `#pragma instance_data_layout Color4f` 类似标注），而非反射 stride。短期可通过 shader 名称或 surface 类型推断。

---

## 四、数据流全貌（改造后）

```
┌──────────────────────────────────────────────────────────────────────┐
│ 定义阶段                                                             │
│                                                                      │
│  InstanceDataLayout enum (C++)  ←→  instance_data/*.glsl (GLSL)     │
│  Color4f = { stride:16 }           struct MaterialInstance { vec4 }  │
│  PBRStandard = { stride:16 }       struct MaterialInstance { ... }   │
│                                                                      │
│  两侧通过 constexpr 注册表保持 stride 一致                            │
└──────────┬──────────────────────────────────┬────────────────────────┘
           │                                  │
┌──────────▼──────────────┐    ┌──────────────▼───────────────────────┐
│ Domain（资源提供方）      │    │ MaterialTemplate（资源消费方）        │
│                          │    │                                      │
│ instance_layout: Color4f │    │ required_layout: Color4f             │
│ tex_array_slots: 0x03    │    │ required_tex_slots: 0x01             │
│   (BaseColor + Normal)   │    │   (BaseColor only)                   │
│                          │    │                                      │
│ mi_data_manager (pool)   │    │ Shader / Pipeline / DescriptorSet    │
│ mi_max_count: 256        │    │                                      │
└──────────┬──────────────┘    └──────────────┬───────────────────────┘
           │                                  │
           └──────────┬───────────────────────┘
                      │
           ┌──────────▼──────────────────────┐
           │ DomainMaterialBinding            │
           │                                  │
           │ 校验 ①: layout enum 一致         │
           │ 校验 ②: (req & avail) == req    │
           │                                  │
           │ PerMaterial DescriptorSet         │
           │   (TextureArray binding etc.)     │
           └──────────┬──────────────────────┘
                      │
           ┌──────────▼──────────────────────┐
           │ Primitive                        │
           │                                  │
           │ material_template*               │
           │ domain*                          │
           │ mi_id (slot in domain pool)      │
           │ mit_packed[] (domain 全集 slots)  │
           │ vil*, render_preset              │
           └──────────┬──────────────────────┘
                      │
           ┌──────────▼──────────────────────┐
           │ ECS 批次处理                     │
           │                                  │
           │ Batch Key: {material, pipeline,  │
           │            domain, queue}        │
           │                                  │
           │ MI Data SSBO ← domain pool       │
           │ MI ID SSBO   ← batch indexing    │
           │ MIT SSBO     ← mit_packed        │
           │ Transform SSBO ← transforms      │
           └──────────┬──────────────────────┘
                      │
           ┌──────────▼──────────────────────┐
           │ GPU Shader                       │
           │                                  │
           │ MID[gl_InstanceIndex] → mi_id    │
           │ MI[mi_id]  → MaterialInstance {} │
           │ MIT[mi_id] → texture layer idx   │
           └──────────────────────────────────┘
```

---

## 五、ShadowMap 多材质共域場景示例

一个典型场景：同一批角色，需要同时用 PBR 材质渲染正常画面 + ShadowMap 材质渲染深度。

```
Domain（角色实例池）:
    instance_layout: PBRStandard
    tex_array_slots: BaseColor | Normal | OpacityMask   (0b01000011)
    mi_max_count: 128

PBR Material:
    required_layout: PBRStandard
    required_tex_slots: BaseColor | Normal | OpacityMask (0b01000011)
    → 校验 (0x43 & 0x43) == 0x43 ✓

ShadowMap Material:
    required_layout: PBRStandard    ← 同布局（需要读取 opacity 阈值）
    required_tex_slots: OpacityMask  (0b01000000)
    → 校验 (0x40 & 0x43) == 0x40 ✓

DMB_PBR    = DomainMaterialBinding(domain, pbr_material)    → binds BaseColor/Normal/OpacityMask arrays
DMB_Shadow = DomainMaterialBinding(domain, shadow_material) → binds OpacityMask array only
```

同一个 Primitive 在不同 RenderPass 中切换 DMB：

```cpp
// 正常渲染
primitive->ChangeBinding(dmb_pbr);     // 指针切换，零开销
// ShadowMap 渲染
primitive->ChangeBinding(dmb_shadow);  // 同域 → 允许；mi_id 不变
```

MIT packed 数组始终按 Domain 全集分配（3 个 slot），ShadowMap Material 只读 offset[OpacityMask] 处的值，其余不访问。

---

## 六、实施分阶段

### Phase A — 创建 InstanceDataLayout 基础设施

**新建文件**：
- `inc/hgl/mtl/InstanceDataLayout.h`

**新建 GLSL 文件**（权威结构体）：
- `ShaderLibrary/instance_data/Color4f.glsl`
- `ShaderLibrary/instance_data/PBRColor.glsl`
- `ShaderLibrary/instance_data/PBRStandard.glsl`
- `ShaderLibrary/instance_data/TextureBlinnPhong.glsl`

**完成标志**：enum + constexpr 注册表编译通过，无任何运行时影响。

### Phase B — Domain 内部改造

**修改**：
- `VKMaterialResourceDomain.h/cpp`：构造函数、`GetLayout()`、`GetTextureArraySlots()`
- `MaterialManager.cpp`：Domain 创建路径改为传 `InstanceDataLayout` + `max_count` + `tex_array_slots`

**保持**：`GetMIDataBytes()` 接口不变（内部查表），下游零改动。

**完成标志**：Domain 不再持有 `MaterialTemplate*` 引用；`MaterialResourceDomain(MaterialTemplate*)` 构造函数删除。

### Phase C — MaterialTemplate 需求声明改造

**修改**：
- `VKMaterialTemplate.h`：`mi_data_bytes` → `required_instance_layout`；`texture_array_slot_flags` → `required_texture_array_slots`
- `MaterialFinalizeFlowAdapter.cpp`：stride → layout 映射
- `MaterialManager.cpp`：`SetTextureArraySlotFlags` 调用点更新

**保持**：`GetMIDataBytes()`、`hasMI()` 接口不变。

**完成标志**：MaterialTemplate 无 byte-stride 成员；通过 enum 声明需求。

### Phase D — DomainMaterialBinding 校验升级

**修改**：
- `VKDomainMaterialBinding.cpp` / `MaterialManager.cpp`（创建 DMB 处）：双重校验 (enum + bitmask)

**完成标志**：DMB 创建用语义级校验替代字节级匹配。

### Phase E — MIT 来源切换

**修改**：
- `Primitive.cpp`：`InitMITLayout()` 来源从 `material->GetTextureArraySlotFlags()` → `domain->GetTextureArraySlots()`
- `PrimitiveBatchPipeline.cpp`：诊断日志更新
- `MaterialInstanceAssignmentBuffer.cpp`：MIT 校验更新

**完成标志**：MIT 数据长度由 Domain 全集决定；变种切换无需重分配。

### Phase F — Shader 层整理（可独立、可延后）

**修改**：各 `*_surface.glsl` 中的 inline `struct MaterialInstance` 改为 `#include "instance_data/XXX.glsl"`。

**完成标志**：每种布局只有一处权威声明。

---

## 七、依赖关系

```
Phase A (独立)       InstanceDataLayout 基础设施
    │
    ├──→ Phase B     Domain 内部改造
    │        │
    │        ├──→ Phase D    DMB 校验升级
    │        │
    │        └──→ Phase E    MIT 来源切换
    │
    └──→ Phase C     MaterialTemplate 需求声明改造
             │
             └──→ Phase D    DMB 校验升级

Phase F (独立)       Shader 层整理（可随时进行）
```

Phase A → B/C 可并行；Phase D 依赖 B + C 都完成；Phase E 依赖 B 完成。

---

## 八、现有 GLSL 布局盘点

当前 `ShaderLibrary/surface/` 中各 surface shader 内联的 `struct MaterialInstance`：

| Surface 文件 | 字段 | stride | 对应 enum |
|---|---|---|---|
| `gizmo3d_surface.glsl` | `vec4 color` | 16 | `Color4f` |
| `unlit_color3d_surface.glsl` | `vec4 color` | 16 | `Color4f` |
| `unlit_luminance_surface.glsl` | `vec4 color` | 16 | `Color4f` |
| `pbrcolor3d_surface.glsl` | `uint base_color; float metallic; float roughness` | 12 | `PBRColor` |
| `standard_surface.glsl` | `uint base_color; float metallic; float roughness; float normal_scale` | 16 | `PBRStandard` |
| `textureblinnphong_surface.glsl` | `float normal_strength` | 4 | `TextureBlinnPhong` |

> **注意 stride 碰撞**：`Color4f` 和 `PBRStandard` 都是 16 bytes。这正是 enum 语义校验比字节匹配更安全的原因——纯靠 stride 无法区分这两种完全不同的数据格式。

---

## 九、风险与缓解

| 风险 | 缓解 |
|------|------|
| stride 碰撞导致 `ResolveLayoutFromStride()` 误判 | 短期通过 surface 名推断 layout；长期让 shader 编译阶段输出 enum 标注 |
| enum 值增多后维护负担 | 保持 enum 精简——只注册实际使用的布局；罕见/实验性布局可走 `Custom` 逃生口（手动传 stride） |
| Phase B/C 并行开发冲突 | B 和 C 修改不同文件（Domain vs Template），冲突面小 |
| MIT 改用 Domain 全集后 packed 数组变大 | 最多 `SamplerSlotCount`（当前 8）个 uint32，增量 ≤ 32 bytes/Primitive，可忽略 |
| 现有 `SetTextureArraySlotFlags` 调用点多 | 逐一 grep 更新；接口改名后编译器会报所有遗漏点 |

---

## 十、不变列表

以下设计经验证正确，本次改造中保持不变：

| 设计 | 理由 |
|------|------|
| `PrimitiveMaterialSlot` 作为 POD 传输 | ECS 热路径零间接，Phase 2c 验证有效 |
| `DomainMaterialBinding` 三元组 (domain + template + PerMat MP) | 正确的绑定单元 |
| Domain Handle Table (domain_id + generation) | 防悬垂指针 |
| Batch Key = {material, pipeline, domain, queue} | Domain 参与批次分组正确 |
| MIT SSBO 独立于 MI Data SSBO | 两者正交，生命周期不同 |
| `GetMIDataBytes()` 返回 `uint32_t` | 下游消费者只关心字节数，接口不变 |
