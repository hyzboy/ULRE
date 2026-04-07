# 材质系统重构计划 v2

> 2026-04-07 · 基于 MaterialTemplate / MaterialResourceDomain / DomainMaterialBinding / MaterialInstance / Primitive / PrimitiveComponent 全链路分析

---

## 零、当前完成进度（2026-04-07）

| Phase | 标题 | 状态 | 备注 |
|-------|------|------|------|
| **Phase 1** | `MaterialTemplate` 瘦身：移出 MI 工厂 | ✅ 完成 | 见下文 |
| **Phase 2a** | `Primitive` 吸收 MI 字段 | ✅ 完成 | 见下文（有设计偏差，已记录） |
| **Phase 2b** | 更新所有 MI 调用点 | ⬜ 未开始 | |
| **Phase 2c** | 删除 `MaterialInstance` 类 | ⬜ 未开始 | |
| **Phase 3** | `PrimitiveComponent` 简化 | ⬜ 未开始 | |
| **Phase 4** | `Primitive::ChangeBinding` 域一致性 guard | ⬜ 未开始（接口已在 2a 中准备） | |
| **Phase 5** | 拆分 `MaterialAssetRegistry`，新增 `SemanticMaterialVariantManager` | ⬜ 未开始 | |

### Phase 1 完成详情

**已删除（从 `VKMaterialTemplate.h/cpp`）**：
- `MaterialResourceDomain *default_domain` 成员
- `CreateMI(MaterialManager*, const VIL*)` / `CreateMI(MaterialManager*, const VILConfig*)`
- `ReleaseMI(int)` / `GetMIData(int)` / `default_domain` 相关任何访问

**已迁入 `MaterialManager`**：
- `std::unordered_map<MaterialTemplate*, MaterialResourceDomain*> default_domain_map`
- `GetOrCreateDefaultDomain(MaterialTemplate*)` 方法
- `AcquireMaterialInstance` 的 `spec.domain==nullptr` 分支现通过 `GetOrCreateDefaultDomain` + `CreateMaterialInstance` 走新路径

**构建验证**：`ULRE.SceneGraph`、`ULRE.ECS`、`CMCore`、`CMAssetsManage` 全部 0 错误（仅 WasmCScript codegen 的已知无关错误存在）。

### Phase 2a 完成详情

**设计偏差（重要）**：  
计划原稿中 Phase 2a 使用 `DomainMaterialBinding* binding` 作为 Primitive 的核心字段，但实施后发现 `DomainMaterialBinding::GetPerMaterialMP()` 在整个 ECS 热路径中 **零调用者**（`BindDescriptorSets(DomainMaterialBinding*)` 方法定义存在但从未被调用），因此本阶段改为直接存储 `MaterialTemplate*` + `MaterialResourceDomain*`，不引入 DMB 指针。Phase 2c 完成后视情况决定是否引入 `binding*`。

**已修改文件**：

`inc/hgl/vk/VKMaterialInstance.h`：
- 新增 `friend class Primitive;` 以允许 `BindMaterialInstance` 访问 protected MIT 字段

`inc/hgl/graph/mesh/Primitive.h`：
- 新增 8 个直接字段：`material_template*`、`domain*`、`mi_id`、`vil*`、`render_preset`、`mit_slot_offset[]`、`mit_packed_count`、`mit_packed*`
- `mat_inst` 保留为 `@deprecated` Phase 2c 桥接字段
- `GetPipelineLayout()` / `GetMaterial()` 改为直接访问 `material_template`（消除双重间接）
- 新增完整访问接口：`GetMaterialTemplate()`、`GetDomain()`、`GetVIL()`、`GetMIID()`、`GetRenderPreset()`、`SetRenderPreset()`、`GetMIData()`、`WriteMIData()`、`InitMITLayout()`、`SetTextureArrayLayer()`、`GetTextureArrayLayer()`
- `HasDeferredMI()` 改为检查 `material_template==nullptr`
- `ChangeMaterialInstance()` 同步更新所有 5 个直接字段

`src/SceneGraph/mesh/Primitive.cpp`：
- `Primitive(Geometry*, MaterialInstance*, ...)` 构造函数：从 MI 复制所有字段（包括克隆 MIT packed 数组），通过 `friend` 直接访问 protected 成员
- `Primitive(Geometry*, SemanticMaterialId, uint32_t)` 构造函数：零初始化 `mit_slot_offset`
- `~Primitive()`：增加 `delete[] mit_packed`
- `UpdateGeometry()`：改用成员 `vil` 替代 `mat_inst->GetVIL()`
- `BindMaterialInstance()`：绑定后同步所有直接字段 + 克隆 MIT 数组
- 新增方法实现：`WriteMIData()`、`InitMITLayout()`、`SetTextureArrayLayer()`、`GetTextureArrayLayer()`

**构建验证**：同 Phase 1，所有主要模块 0 错误。

---

## 一、当前架构

```
                          SemanticMaterialId
                                 │
                   MaterialAssetRegistry.semantic_cache
                                 │
            ┌────────────────────┼─────────────────────┐
            ▼                    ▼                      ▼
   MaterialTemplate      MaterialResourceDomain    DomainMaterialBinding
   (shader契约+管线布局)  (MI数据池)                (domain + template + PerMat MP)
            │                    │
            └──────┬─────────────┘
                   ▼
            MaterialInstance   ← 中间包装层
            (material_manager* → Map间查template)
            (domain*)
            (vil*, mi_id, render_preset, MIT data)
                   │
                   ▼
              Primitive
              (mat_inst*, geometry*)
                   │
                   ▼
           PrimitiveComponent
           (primitive*, overrideMaterial: MaterialInstance*)
```

### 核心问题

| # | 问题 | 详述 |
|---|---|---|
| **P1** | `MaterialInstance` 与 `DomainMaterialBinding` 职责重叠 | DMB 已经是 (domain, template, PerMaterial MP) 的三元组；MI 又包了一层 domain + 间接查 template，两者共存是冗余 |
| **P2** | `MaterialInstance` 只是一个胖 struct，无独立行为 | 全部 MI 方法调用可拆解为：`binding->GetDomain()->GetMIData(mi_id)` + `binding->GetMaterial()` + 简单成员读取；无子类化 |
| **P3** | `Primitive` 通过 MI 间接访问一切 | `GetPipelineLayout()` = `mat_inst->GetMaterial()->GetPipelineLayout()`，两层间接 |
| **P4** | `PrimitiveComponent` 持有 `overrideMaterial: MaterialInstance*` | 与 `primitive->mat_inst` 形成平行路径，语义不清；应由 ECS 系统控制变种切换，不在组件上挂两个 MI 指针 |
| **P5** | `MaterialTemplate` 仍含 MI 工厂 | `CreateMI`/`ReleaseMI`/`GetMIData`/`default_domain` 不该属于模板类 |
| **P6** | `ChangeMaterialInstance` guard 以 `MaterialTemplate*` 身份比较 | 多变种共享域时所有合法切换被阻止 |
| **P7** | `MaterialAssetRegistry` 职责过重 | 混合了资产加载、变种生成、实体级 MI 缓存；将来变种类型增加后难以维护 |

---

## 二、目标架构

```
                          SemanticMaterialId
                                 │
              SemanticMaterialVariantManager
              (semantic_id → VariantSet: shared_domain + [DMB per variant])
                                 │
            ┌────────────────────┼─────────────────────┐
            ▼                    ▼                      ▼
   MaterialTemplate      MaterialResourceDomain    DomainMaterialBinding
   (shader契约, 只读)     (MI数据池)                (domain + template + PerMat MP)
                                                        │
                                                        ▼
                                                   Primitive
                                                   (geometry*)
                                                   (binding: DomainMaterialBinding*)
                                                   (mi_id, vil*, render_preset)
                                                   (mit_slot_offset, mit_packed)
                                                        │
                                                        ▼
                                                 PrimitiveComponent
                                                 (primitive*)
                                                 (semantic_id, active_variant)
```

**关键变化**：

1. **删除 `MaterialInstance` 类** — 其数据直接内联到 `Primitive`
2. **`Primitive` 直接持有 `DomainMaterialBinding*`** — 一跳访问 template / domain / PerMat MP
3. **`PrimitiveComponent` 不再持有任何材质指针** — 只持有语义标签 + 变种选择
4. **`MaterialTemplate` 成为纯只读契约** — MI 工厂移入 `MaterialManager`
5. **新增 `SemanticMaterialVariantManager`** — 语义变种的统一管理层

---

## 三、Draw Call 视角验证

一次 Vulkan Draw Call 需要的全部数据：

| 数据 | 新方案来源 | 旧方案来源 |
|---|---|---|
| `VkPipeline` | `Primitive.binding->GetMaterial()` + `Primitive.vil` + `Primitive.render_preset` | `mi->GetMaterial()` + `mi->GetVIL()` + `mi->GetRenderPreset()` |
| `VkPipelineLayout` | `Primitive.binding->GetPipelineLayout()` | `mi->GetMaterial()->GetPipelineLayout()` |
| DescSet PerMaterial | `Primitive.binding->GetPerMaterialMP()` | 需额外查 DMB |
| DescSet PerObject (MI data) | `Primitive.binding->GetDomain()->GetMIData(Primitive.mi_id)` | `mi->GetMIData()` → `domain->GetMIData(mi_id)` |
| MIT 纹理层数据 | `Primitive.mit_packed` / `Primitive.mit_packed_count` | `mi->GetMITData()` / `mi->GetMITDataBytes()` |
| VertexBuffers | `Primitive.geometry` + `Primitive.vil` | 同 |
| IndexBuffer | `Primitive.geometry` | 同 |
| DrawRange | `Primitive.draw_range` | 同 |

**结论**：原 `MaterialInstance` 的每个字段都有直接对应，无信息丢失。

---

## 四、分阶段实施

### Phase 1 — `MaterialTemplate` 瘦身：移出 MI 工厂

**目标**：`MaterialTemplate` 成为纯只读 shader 契约，不持有任何运行时状态。

**变动文件**：
- `inc/hgl/vk/VKMaterialTemplate.h`
- `src/SceneGraph/VKMaterialTemplate.cpp`
- `inc/hgl/graph/module/MaterialManager.h`
- `src/SceneGraph/module/MaterialManager.cpp`

**删除（从 `MaterialTemplate`）**：
```cpp
MaterialResourceDomain *default_domain;
void ReleaseMI(int);
void *GetMIData(int);
MaterialInstance *CreateMI(MaterialManager *, const VIL *);
MaterialInstance *CreateMI(MaterialManager *, const VILConfig *);
```

**保留（`MaterialTemplate` 只读查询）**：
```cpp
const uint32_t GetMIDataBytes() const;
const uint32_t GetMIMaxCount()  const;
const VIL *    GetDefaultVIL()  const;
VIL *          CreateVIL(const VILConfig * = nullptr);
bool           Release(VIL *);
```

**迁入 `MaterialManager`**：
- `default_domain` → `unordered_map<MaterialTemplate*, MaterialResourceDomain*> default_domain_map`
- 所有 MI 创建统一走 `MaterialManager::AcquireMaterialInstance(spec)`

**完成标志**：`MaterialTemplate` 无任何 `MaterialInstance`、`MaterialResourceDomain` 成员。

---

### Phase 2 — 将 MI 字段内联到 `Primitive`，删除 `MaterialInstance`

这是整个重构的核心阶段。拆为 3 个子步骤。

#### Phase 2a — `Primitive` 吸收 MI 字段

**变动文件**：
- `inc/hgl/graph/mesh/Primitive.h`
- `src/SceneGraph/mesh/Primitive.cpp`

**Primitive 新成员**：
```cpp
class Primitive
{
    Geometry *geometry;
    GeometryDataBuffer *data_buffer;
    GeometryDrawRange   draw_range;

    // ── 原 MaterialInstance 的全部职责 ──
    DomainMaterialBinding *binding = nullptr;   // domain + template + PerMat MP
    int                    mi_id   = -1;        // 域内数据槽 ID
    const VIL             *vil     = nullptr;   // 顶点布局
    GraphicsPipelinePreset render_preset = GraphicsPipelinePreset::Solid3D;

    // MIT (纹理数组层索引)
    int8_t   mit_slot_offset[mtl::SamplerSlotCount];
    uint32_t mit_packed_count = 0;
    uint32_t *mit_packed      = nullptr;

    // 延迟绑定
    SemanticMaterialId deferred_semantic_id = 0;
    uint32_t           deferred_vil_hash    = 0;
};
```

**Primitive 新接口**：
```cpp
// 只读访问
MaterialTemplate       *GetMaterialTemplate() const { return binding ? binding->GetMaterial() : nullptr; }
MaterialResourceDomain *GetDomain()           const { return binding ? binding->GetDomain()   : nullptr; }
VkPipelineLayout        GetPipelineLayout()   const { return binding ? binding->GetPipelineLayout() : VK_NULL_HANDLE; }
DomainMaterialBinding  *GetBinding()          const { return binding; }
const VIL             *GetVIL()              const { return vil; }
int                     GetMIID()             const { return mi_id; }
GraphicsPipelinePreset  GetRenderPreset()     const { return render_preset; }

// MI 数据读写（直接委托 domain）
void *GetMIData() { return (binding && mi_id >= 0) ? binding->GetDomain()->GetMIData(mi_id) : nullptr; }

template<typename T>
void WriteMIData(const T &data) { WriteMIData(&data, sizeof(T)); }
void WriteMIData(const void *data, uint32_t size);

// MIT 纹理数组层
uint32_t GetMITDataBytes() const { return mit_packed_count * sizeof(uint32_t); }
void    *GetMITData()      { return mit_packed; }
void     InitMITLayout(uint8_t slot_flags);
void     SetTextureArrayLayer(mtl::SamplerSlot slot, uint32_t layer);
uint32_t GetTextureArrayLayer(mtl::SamplerSlot slot) const;

// 绑定管理（替代 ChangeMaterialInstance）
bool ChangeBinding(DomainMaterialBinding *new_binding)
{
    if (!new_binding) return false;
    if (!binding) { binding = new_binding; return true; }
    // 同域 = 数据池兼容 = 可以安全切换 Template 变种
    if (new_binding->GetDomain() != binding->GetDomain()) return false;
    binding = new_binding;
    return true;
}
```

**兼容层**：在过渡期保留 `GetMaterial()` 别名：
```cpp
[[deprecated("use GetMaterialTemplate()")]]
MaterialTemplate *GetMaterial() { return GetMaterialTemplate(); }
```

#### Phase 2b — 更新所有调用点

**按消费场景逐一替换**：

| 旧调用（通过 MI） | 新调用（直接 Primitive） | 涉及文件 |
|---|---|---|
| `mi->GetMaterial()` | `prim->GetMaterialTemplate()` | Primitive.cpp, CollectSystem, BatchPipeline |
| `mi->GetDomain()` | `prim->GetDomain()` | BatchPipeline (batch key), MIAB (diagnostics) |
| `mi->GetVIL()` | `prim->GetVIL()` | CollectSystem, BatchPipeline |
| `mi->GetMIID()` | `prim->GetMIID()` | MIAB (WriteItems: per-draw index) |
| `mi->GetMIData()` | `prim->GetMIData()` | CollectSystem (fallback), MIAB (SSBO upload) |
| `mi->GetMITDataBytes()` | `prim->GetMITDataBytes()` | MIAB |
| `mi->GetMITData()` | `prim->GetMITData()` | MIAB (MIT SSBO upload) |
| `mi->GetRenderPreset()` | `prim->GetRenderPreset()` | CollectSystem, BatchPipeline |
| `prim->GetMaterialInstance()` | `prim->GetBinding()` + `prim->GetMIID()` | 全局 |

**核心 ECS 文件清单**：
- `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp`
- `src/ecs/systems/render/RenderDescriptorBindingSystem.cpp`
- `src/ecs/support/PrimitiveBatchPipeline.cpp`
- `src/ecs/support/MaterialInstanceAssignmentBuffer.cpp`
- `src/ecs/core/PrimitiveRenderItem.cpp`
- `src/ecs/components/PrimitiveComponent.cpp`

#### Phase 2c — 删除 `MaterialInstance`

- 删除 `inc/hgl/vk/VKMaterialInstance.h`
- 删除 `src/SceneGraph/VKMaterialInstance.cpp`
- `MaterialManager` 中删除：
  - `rm_material_instance`
  - `material_instance_material_map`
  - `BindInstanceMaterial` / `ForgetInstanceMaterial` / `ResolveMaterial`
  - `MaterialInstanceID` / `MaterialInstanceSpec` / `MaterialInstanceSpecKey` / `MaterialInstanceAcquireStats`
  - `AcquireMaterialInstance()` → 替换为 `AllocPrimitiveMISlot()`（给 Primitive 分配 mi_id）

- `MaterialAssetRegistry` 中：
  - `entity_mi_cache` / `legacy_final_mi_cache` → 改为缓存 `(DomainMaterialBinding*, mi_id)` 对
  - `ResolveMI` / `AcquireMI` / `CreateMI` 返回值从 `MaterialInstance*` 改为新的轻量 struct

**MI 槽位分配替代方案**：
```cpp
// 新 struct（替代 MaterialInstance*）
struct PrimitiveMaterialSlot
{
    DomainMaterialBinding *binding = nullptr;
    int mi_id = -1;
    const VIL *vil = nullptr;
    GraphicsPipelinePreset preset = GraphicsPipelinePreset::Solid3D;
};

// MaterialManager 新接口
PrimitiveMaterialSlot AllocPrimitiveMISlot(DomainMaterialBinding *binding,
                                            const VIL *vil,
                                            GraphicsPipelinePreset preset);
void FreePrimitiveMISlot(const PrimitiveMaterialSlot &slot);
```

**完成标志**：工程中不再存在 `MaterialInstance` 类型；所有渲染路径通过 `Primitive` 直接访问 DMB。

---

### Phase 3 — `PrimitiveComponent` 简化

**变动文件**：
- `inc/hgl/ecs/components/PrimitiveComponent.h`
- `src/ecs/components/PrimitiveComponent.cpp`

**删除**：
```cpp
// 删除
hgl::graph::MaterialInstance* overrideMaterial;
void SetOverrideMaterial(hgl::graph::MaterialInstance* mi);
hgl::graph::MaterialInstance* GetOverrideMaterial() const;
void ClearOverrideMaterial();
hgl::graph::MaterialInstance* GetMaterialInstance() const;
```

**保留/修改**：
```cpp
class PrimitiveComponent : public RenderableComponent
{
    hgl::graph::Primitive *primitive;
    hgl::graph::SemanticMaterialId semanticMaterialId = 0;

    // 新增：ECS 控制的变种选择
    uint8_t active_variant_type = 0;  // 0=opaque, 1=dither, 2=alpha_blend, 16+=VFX变种

public:
    // Primitive access
    void SetPrimitive(hgl::graph::Primitive *prim);
    hgl::graph::Primitive *GetPrimitive() const { return primitive; }

    // Semantic material + variant
    void SetSemanticMaterial(hgl::graph::SemanticMaterialId id) { semanticMaterialId = id; }
    hgl::graph::SemanticMaterialId GetSemanticMaterial() const { return semanticMaterialId; }
    bool HasSemanticMaterial() const { return semanticMaterialId != 0; }
    void SetActiveVariant(uint8_t v) { active_variant_type = v; }
    uint8_t GetActiveVariant() const { return active_variant_type; }

    // 材质信息委托给 Primitive
    hgl::graph::MaterialTemplate *GetMaterial() const;   // → primitive->GetMaterialTemplate()
    bool CanRender() const;                               // → primitive && primitive->GetBinding()
};
```

**ECS 变种切换路径**（由 `RenderPrimitiveCollectSystem` 实现）：
```cpp
// 在 collect 阶段
auto *variant_mgr = ...;  // SemanticMaterialVariantManager
auto *binding = variant_mgr->GetVariantBinding(comp->GetSemanticMaterial(),
                                                comp->GetActiveVariant());
primitive->ChangeBinding(binding);  // 同域内安全切换
```

**完成标志**：`PrimitiveComponent` 中无任何 `MaterialInstance` 引用。

---

### Phase 4 — `Primitive::ChangeBinding` 域一致性 guard

**已在 Phase 2a 中内联**：

```cpp
bool ChangeBinding(DomainMaterialBinding *new_binding)
{
    if (!new_binding) return false;
    if (!binding) { binding = new_binding; return true; }
    if (new_binding->GetDomain() != binding->GetDomain()) return false;
    binding = new_binding;
    return true;
}
```

**语义**：
- 同域 → 数据池兼容 → opaque/dither/blend/stone/ghost/xray 自由切换 ✓
- 跨域 → 数据池不兼容 → 拒绝（mi_id 属于旧域，不能跨域使用）✓
- `nullptr` → 拒绝 ✓

如需跨域切换（更换资源集合），需调用 `MaterialManager` 重新分配 mi_id + binding。

---

### Phase 5 — 拆分 `MaterialAssetRegistry`，新增 `SemanticMaterialVariantManager`

**新文件**：
- `inc/hgl/graph/module/SemanticMaterialVariantManager.h`
- `src/SceneGraph/module/SemanticMaterialVariantManager.cpp`

**`SemanticMaterialVariantManager` 核心结构**：
```cpp
enum class MaterialVariantType : uint8_t
{
    Opaque       = 0,
    Dither       = 1,
    AlphaBlend   = 2,
    // 手动注册 VFX 变种
    Stone        = 16,
    Ghost        = 17,
    XRay         = 18,
    NightVision  = 19,
    IRThermal    = 20,
};

struct SemanticVariantSet
{
    MaterialResourceDomain *shared_domain = nullptr;
    unordered_map<MaterialVariantType, DomainMaterialBinding*> bindings;
};

class SemanticMaterialVariantManager
{
    unordered_map<SemanticMaterialId, SemanticVariantSet> variant_sets;

public:
    // 注册变种
    DomainMaterialBinding *GetOrCreateVariant(
        SemanticMaterialId id,
        MaterialVariantType type,
        MaterialTemplate *tmpl);

    // 查询共享域
    MaterialResourceDomain *GetSharedDomain(SemanticMaterialId id);

    // 获取变种的 DMB（ECS 变种切换用）
    DomainMaterialBinding *GetVariantBinding(SemanticMaterialId id, uint8_t variant_type);

    // 为 Primitive 分配 MI 槽位 + 绑定到变种 DMB
    PrimitiveMaterialSlot AllocSlot(SemanticMaterialId id,
                                     MaterialVariantType type,
                                     const VIL *vil,
                                     GraphicsPipelinePreset preset);
};
```

**`MaterialAssetRegistry` 收缩为**：
- 资产记录缓存（`semantic_cache` 只存 `MaterialAssetRecord`）
- `MaterialTemplate` / `MaterialResourceDomain` / `DomainMaterialBinding` 的底层工厂委托
- 删除 `variant_cache` / `entity_mi_cache` / `legacy_final_mi_cache`（迁入 `SemanticMaterialVariantManager`）

---

## 五、实施顺序与依赖

```
Phase 1 (独立)                    MaterialTemplate 瘦身
    │
    ▼
Phase 2a (依赖 1)                 Primitive 吸收 MI 字段
    │
    ▼
Phase 2b (依赖 2a)                更新所有 MI 调用点 → Primitive
    │
    ▼
Phase 2c (依赖 2b)                删除 MaterialInstance 类
    │
    ├──────────┐
    ▼          ▼
Phase 3      Phase 4              PrimitiveComponent 简化 + ChangeBinding guard
    │          │
    └────┬─────┘
         ▼
Phase 5                           SemanticMaterialVariantManager 拆分
```

Phase 1 可独立先行；Phase 3 和 Phase 4 可在 Phase 2 完成后并行；Phase 5 最后实施。

---

## 六、不变列表（正确设计，保留）

| 设计 | 理由 |
|---|---|
| `DomainMaterialBinding` 三元组 | domain + template + PerMat MP，是正确的绑定单元 |
| `MaterialResourceDomain` 不持有 `MaterialTemplate*` | 域与模板完全解耦，是多变种共享域的基础 |
| `MaterialManager` 统一管理生命周期 | Template / Domain / DMB 的创建和销毁归 Manager |
| `SemanticMaterialId` 作为稳定 key | 贯穿全链路 |
| `VIL` 独立于 Template | 顶点布局是几何体属性，不是材质属性 |
| `Geometry` 与材质无关 | 纯几何数据，绑定在 Primitive 层面 |

---

## 七、预期收益

| 收益 | 说明 |
|---|---|
| **删除一个核心类** | `MaterialInstance`（~180行头文件 + ~120行实现）消失，系统从三层变两层 |
| **消除 `material_instance_material_map`** | 不再需要 Map 间接查找 Template |
| **`Primitive` 成为真正的最小渲染单元** | 几何 + 绑定 + 数据槽 = 一次 draw call 所需的一切 |
| **变种切换开销降低** | `ChangeBinding` 只是替换一个指针，而非创建/销毁 MI 对象 |
| **`PrimitiveComponent` 语义清晰** | 只持有 Primitive + 语义标签，不再有平行的 MI 引用 |
| **为 VFX 变种铺路** | 石雕/幽灵/X光/夜视/红外共享域，通过 `SemanticMaterialVariantManager` 管理 |

---

## 八、风险与缓解

| 风险 | 缓解措施 |
|---|---|
| Phase 2 影响面大（全局 MI 调用点更换） | Phase 2 拆为 a/b/c 三步；2a 先保留 `GetMaterialInstance()` 兼容层 |
| MIT 纹理数组数据内联到 Primitive 增加 Primitive 体积 | MIT 字段本身很小（slot_offset 8字节 + packed_count 4字节 + 指针 8字节）；且原来也是 per-object 数据 |
| `MaterialAssetRegistry` 的 `ResolveMI` / `AcquireMI` 返回值类型变化 | 引入 `PrimitiveMaterialSlot` 值类型作为过渡；旧签名标记 `[[deprecated]]` |
| `MaterialInstanceAssignmentBuffer` 依赖 `mi_set.Find(mi)` 做 MI→index 映射 | 改为 `primitive_set.Find(prim)` 或直接用 `prim->GetMIID()` |
