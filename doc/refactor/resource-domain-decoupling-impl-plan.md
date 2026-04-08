# ResourceDomain 解耦 — 详细实施计划

> 对应设计文档：`doc/refactor/resource-domain-decoupling.md`
> 2026-04-08 · 基于当前代码库精确定位

---

## 总览

本重构将 `MaterialResourceDomain` 从对 `MaterialTemplate` 的隐式 byte-stride 耦合中解放出来，引入 `InstanceDataLayout` 枚举作为语义级数据格式契约，并将 TextureArray slot flags 从需求方（Material）迁移到供给方（Domain）。

**6 个阶段**：A → B → C → D → E → F，其中 A 无运行时影响，F 可独立排期。

---

## Phase A — 创建 InstanceDataLayout 基础设施

**目标**：引入 enum + constexpr 注册表 + GLSL 权威结构体文件。纯新增，零运行时影响。

### A.1 新建 `inc/hgl/mtl/InstanceDataLayout.h`

```cpp
#pragma once

#include <cstdint>
#include <hgl/type/EnumClassRange.h>

namespace hgl::graph::mtl
{

enum class InstanceDataLayout : uint8_t
{
    None = 0,           ///< 无实例数据 (stride = 0)
    Color4f,            ///< vec4 color — Gizmo3D, Unlit 着色 (16B)
    PBRColor,           ///< uint base_color + float metallic + float roughness — PBR 打包色 (12B)
    PBRStandard,        ///< uint base_color + float metallic + float roughness + float normal_scale — PBR 标准 (16B)
    TextureBlinnPhong,  ///< float normal_strength — 纹理 Blinn-Phong (4B)

    ENUM_CLASS_RANGE(None, TextureBlinnPhong)
};

struct InstanceDataLayoutInfo
{
    uint32_t    stride;         ///< 单个实例数据字节数
    const char *name;           ///< 调试名
    const char *glsl_struct;    ///< 对应 GLSL struct 名（用于校验/文档）
};

inline constexpr InstanceDataLayoutInfo kInstanceDataLayouts[] =
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

} // namespace hgl::graph::mtl
```

### A.2 新建 GLSL 权威结构体文件

以下文件放在 `ShaderLibrary/instance_data/` 目录下：

#### `ShaderLibrary/instance_data/Color4f.glsl`
```glsl
// instance_data/Color4f.glsl
// InstanceDataLayout::Color4f — stride = 16 bytes
struct MaterialInstance
{
    vec4 color;         // 16 bytes
};
```

#### `ShaderLibrary/instance_data/PBRColor.glsl`
```glsl
// instance_data/PBRColor.glsl
// InstanceDataLayout::PBRColor — stride = 12 bytes
struct MaterialInstance
{
    uint  base_color;   // 4 bytes (packed RGBA)
    float metallic;     // 4 bytes
    float roughness;    // 4 bytes
};
```

#### `ShaderLibrary/instance_data/PBRStandard.glsl`
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

#### `ShaderLibrary/instance_data/TextureBlinnPhong.glsl`
```glsl
// instance_data/TextureBlinnPhong.glsl
// InstanceDataLayout::TextureBlinnPhong — stride = 4 bytes
struct MaterialInstance
{
    float normal_strength;  // 4 bytes
};
```

### A.3 验证

- [ ] 新头文件被 `#include` 后编译通过（仅 constexpr，无链接开销）
- [ ] GLSL 文件存在，内容与 `kInstanceDataLayouts[]` 的 stride 一致
- [ ] 现有代码**零改动**，全量编译通过

---

## Phase B — Domain 内部改造

**目标**：`MaterialResourceDomain` 持有 `InstanceDataLayout` enum + `texture_array_slot_flags`，不再依赖 `MaterialTemplate*`。

### B.1 修改 `inc/hgl/vk/VKMaterialResourceDomain.h`

**当前代码** (L26-27, L33-34, L47-49)：
```cpp
    uint32_t  mi_data_bytes     = 0;        ///< 单个 MI 数据 stride
    uint32_t  mi_max_count      = 0;        ///< 渲染批次最大实例数
    // ...
    MaterialResourceDomain(uint32_t mi_bytes, uint32_t mi_count);
    explicit MaterialResourceDomain(MaterialTemplate *mtl);
    // ...
    bool     hasMI()          const { return mi_data_bytes > 0; }
    uint32_t GetMIDataBytes() const { return mi_data_bytes; }
    uint32_t GetMIMaxCount()  const { return mi_max_count; }
```

**改为**：
```cpp
    #include<hgl/mtl/InstanceDataLayout.h>
    // ...
    mtl::InstanceDataLayout  instance_layout         = mtl::InstanceDataLayout::None;
    uint32_t                 mi_max_count             = 0;
    uint8_t                  texture_array_slot_flags = 0;   // 从 MaterialTemplate 迁入（供方）
    // ...
    MaterialResourceDomain(mtl::InstanceDataLayout layout, uint32_t max_count,
                           uint8_t tex_array_slots = 0);
    // 删除: explicit MaterialResourceDomain(MaterialTemplate *mtl);
    // ...
    mtl::InstanceDataLayout GetLayout()          const { return instance_layout; }
    bool     hasMI()                             const { return instance_layout != mtl::InstanceDataLayout::None; }
    uint32_t GetMIDataBytes()                    const { return mtl::GetInstanceDataStride(instance_layout); }
    uint8_t  GetTextureArraySlots()              const { return texture_array_slot_flags; }
    uint32_t GetMIMaxCount()                     const { return mi_max_count; }
```

> `GetMIDataBytes()` 保留接口签名不变（内部查表），所有下游读取者零改动。

**逐行变更清单**：

| 行号 | 变更 | 说明 |
|------|------|------|
| L3 | 新增 `#include<hgl/mtl/InstanceDataLayout.h>` | 引入枚举 |
| L14 | 删除 `class MaterialTemplate;` 前置声明 | 不再需要 |
| L26 | `uint32_t mi_data_bytes = 0` → `mtl::InstanceDataLayout instance_layout = mtl::InstanceDataLayout::None` | 字段替换 |
| L27 | 保持 `mi_max_count` | 不变 |
| 新增 | 添加 `uint8_t texture_array_slot_flags = 0;` | 从 Template 迁入 |
| L33 | 构造函数参数改为 `(mtl::InstanceDataLayout, uint32_t, uint8_t)` | |
| L34 | 删除 `explicit MaterialResourceDomain(MaterialTemplate *mtl)` | 切断依赖 |
| L35 | 删除 `friend class MaterialTemplate;` | 不再需要 |
| L47 | 新增 `GetLayout()` 方法 | 供 DMB 校验使用 |
| L47 | `hasMI()` 改为 `instance_layout != None` | 语义替换 |
| L48 | `GetMIDataBytes()` 内部改为 `GetInstanceDataStride(instance_layout)` | 查表 |
| 新增 | `GetTextureArraySlots()` 方法 | 供方查询接口 |

### B.2 修改 `src/SceneGraph/VKMaterialResourceDomain.cpp`

**当前代码** (L1-19)：
```cpp
#include<hgl/vk/VKMaterialResourceDomain.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/type/ActiveMemoryBlockManager.h>

hgl::graph::MaterialResourceDomain::MaterialResourceDomain(uint32_t mi_bytes, uint32_t mi_count)
{
    mi_data_bytes = mi_bytes;
    mi_max_count  = mi_count;

    if(mi_data_bytes > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(mi_data_bytes);
}

hgl::graph::MaterialResourceDomain::MaterialResourceDomain(hgl::graph::MaterialTemplate *mtl)
    : MaterialResourceDomain(mtl ? mtl->GetMIDataBytes() : uint32_t(0),
                     mtl ? mtl->GetMIMaxCount() : uint32_t(0))
{
}
```

**改为**：
```cpp
#include<hgl/vk/VKMaterialResourceDomain.h>
// 删除: #include<hgl/vk/VKMaterialTemplate.h>   (不再依赖)
#include<hgl/type/ActiveMemoryBlockManager.h>

hgl::graph::MaterialResourceDomain::MaterialResourceDomain(
    hgl::graph::mtl::InstanceDataLayout layout, uint32_t max_count, uint8_t tex_array_slots)
{
    instance_layout          = layout;
    mi_max_count             = max_count;
    texture_array_slot_flags = tex_array_slots;

    const uint32_t stride = mtl::GetInstanceDataStride(layout);
    if(stride > 0)
        mi_data_manager = new hgl::ActiveMemoryBlockManager(stride);
}

// 删除: MaterialResourceDomain(MaterialTemplate *mtl) 委托构造函数
```

**逐行变更清单**：

| 行号 | 变更 | 说明 |
|------|------|------|
| L2 | 删除 `#include<hgl/vk/VKMaterialTemplate.h>` | 切断头文件依赖 |
| L5-11 | 构造函数签名+体改写 | 从 enum+max_count+slots 初始化 |
| L13-18 | 整段删除（委托构造函数） | 不再从 MaterialTemplate* 构造 |

### B.3 修改 `src/SceneGraph/module/MaterialManager.cpp` — Domain 创建路径

**当前代码** (L940-952)：
```cpp
MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(MaterialTemplate *mtl)
{
    if(!mtl)
        return nullptr;

    return CreateMaterialResourceDomain(mtl->GetMIDataBytes(), mtl->GetMIMaxCount());
}

MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(uint32_t mi_data_bytes,
                                                      uint32_t mi_max_count)
{
    return new MaterialResourceDomain(mi_data_bytes, mi_max_count);
}
```

**改为**：
```cpp
MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(
    mtl::InstanceDataLayout layout, uint32_t max_count, uint8_t tex_array_slots)
{
    return new MaterialResourceDomain(layout, max_count, tex_array_slots);
}
```

**过渡性保留**（Phase B 期间可保留旧重载，标记 deprecated，Phase C 后删除）：
```cpp
// DEPRECATED — Phase C 后删除
MaterialResourceDomain *MaterialManager::CreateMaterialResourceDomain(MaterialTemplate *mtl)
{
    if(!mtl) return nullptr;
    // 临时：从 mtl 的 byte stride 反查 layout（有碰撞风险，Phase C 后彻底移除）
    auto layout = ResolveLayoutFromStride(mtl->GetMIDataBytes());
    return CreateMaterialResourceDomain(layout, mtl->GetMIMaxCount(),
                                        mtl->GetTextureArraySlotFlags());
}
```

### B.4 修改 `inc/hgl/graph/module/MaterialManager.h` — 声明更新

**当前代码** (L375, L381)：
```cpp
    MaterialResourceDomain *  CreateMaterialResourceDomain  (MaterialTemplate *mtl);
    MaterialResourceDomain *  CreateMaterialResourceDomain  (uint32_t mi_data_bytes, uint32_t mi_max_count);
```

**改为**：
```cpp
    // DEPRECATED — Phase C 后删除
    MaterialResourceDomain *  CreateMaterialResourceDomain  (MaterialTemplate *mtl);
    // 新接口
    MaterialResourceDomain *  CreateMaterialResourceDomain  (mtl::InstanceDataLayout layout,
                                                             uint32_t max_count,
                                                             uint8_t tex_array_slots = 0);
```

### B.5 修改 `GetOrCreateDefaultDomain` (MaterialManager.cpp L378-387)

**当前代码**：
```cpp
MaterialResourceDomain *MaterialManager::GetOrCreateDefaultDomain(MaterialTemplate *mtl)
{
    if (!mtl || !mtl->hasMI()) return nullptr;
    auto it = default_domain_map.find(mtl);
    if (it != default_domain_map.end()) return it->second;
    MaterialResourceDomain *domain = CreateMaterialResourceDomain(mtl);
    default_domain_map[mtl] = domain;
    return domain;
}
```

**Phase B 暂不改**（调用的是 deprecated 重载）。Phase C MaterialTemplate 改完后，此函数也随之更新：
```cpp
MaterialResourceDomain *MaterialManager::GetOrCreateDefaultDomain(MaterialTemplate *mtl)
{
    if (!mtl || !mtl->hasMI()) return nullptr;
    auto it = default_domain_map.find(mtl);
    if (it != default_domain_map.end()) return it->second;
    MaterialResourceDomain *domain = CreateMaterialResourceDomain(
        mtl->GetRequiredLayout(), mtl->GetMIMaxCount(), 0);  // default Domain 不提供 TextureArray
    default_domain_map[mtl] = domain;
    return domain;
}
```

### B.6 验证

- [ ] 编译通过（deprecated 重载保证旧调用点不报错）
- [ ] `MaterialResourceDomain.h` 不再 `#include VKMaterialTemplate.h`
- [ ] 跑一遍现有 example/测试，行为不变
- [ ] `GetMIDataBytes()` 返回值与之前一致（相同 enum → 相同 stride）

---

## Phase C — MaterialTemplate 需求声明改造

**目标**：MaterialTemplate 持有 `required_instance_layout` (enum) 和 `required_texture_array_slots`，不再持有原始 byte-stride 和供方 slot flags。

### C.1 修改 `inc/hgl/vk/VKMaterialTemplate.h`

**当前代码** (L53-58, L137-142)：
```cpp
    uint32_t mi_data_bytes;             ///<实例数据大小
    uint32_t mi_max_count;              ///<实例一次渲染最大数量限制
    bool has_l2w_matrix;
    uint8_t texture_array_slot_flags = 0; ///< bit N = SamplerSlot(N) uses TextureArray mode
    // ...
        void    SetTextureArraySlotFlags(uint8_t f){texture_array_slot_flags=f;}
    const uint8_t   GetTextureArraySlotFlags()const{return texture_array_slot_flags;}
    const bool      hasMI           ()const{return mi_data_bytes>0;}
    const uint32_t  GetMIDataBytes  ()const{return mi_data_bytes;}
    const uint32_t  GetMIMaxCount   ()const{return mi_max_count;}
```

**改为**：
```cpp
    #include<hgl/mtl/InstanceDataLayout.h>
    // ...
    mtl::InstanceDataLayout required_instance_layout = mtl::InstanceDataLayout::None;
    uint32_t mi_max_count;
    bool has_l2w_matrix;
    uint8_t required_texture_array_slots = 0; ///< 需方：需要哪些 slot 的 TextureArray
    // ...
    mtl::InstanceDataLayout GetRequiredLayout()const{ return required_instance_layout; }
        void    SetRequiredTextureArraySlots(uint8_t f){ required_texture_array_slots=f; }
    const uint8_t   GetRequiredTextureArraySlots()const{ return required_texture_array_slots; }
    const bool      hasMI           ()const{ return required_instance_layout != mtl::InstanceDataLayout::None; }
    const uint32_t  GetMIDataBytes  ()const{ return mtl::GetInstanceDataStride(required_instance_layout); }
    const uint32_t  GetMIMaxCount   ()const{ return mi_max_count; }
```

**逐行变更清单**：

| 位置 | 变更 | 说明 |
|------|------|------|
| 文件头 | 新增 `#include<hgl/mtl/InstanceDataLayout.h>` | |
| L53 | `uint32_t mi_data_bytes` → `mtl::InstanceDataLayout required_instance_layout = mtl::InstanceDataLayout::None` | 字段替换 |
| L58 | `texture_array_slot_flags` → `required_texture_array_slots` | 语义改名 |
| L137 | `SetTextureArraySlotFlags` → `SetRequiredTextureArraySlots` | 方法改名 |
| L138 | `GetTextureArraySlotFlags` → `GetRequiredTextureArraySlots` | 方法改名 |
| L140 | `hasMI()` 条件改为 `!= None` | |
| L141 | `GetMIDataBytes()` 内部改为查表 | |
| 新增 | `GetRequiredLayout()` 方法 | 供 DMB 校验使用 |

### C.2 修改 `src/SceneGraph/VKMaterialTemplate.cpp`

**当前代码** (L27-28)：
```cpp
    mi_data_bytes=0;
    mi_max_count=0;
```

**改为**：
```cpp
    required_instance_layout = mtl::InstanceDataLayout::None;
    mi_max_count = 0;
```

### C.3 修改 `inc/hgl/graph/module/MaterialFinalizeFlowAdapter.h`

**当前代码** (L18-20)：
```cpp
    struct MaterialFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
        uint32_t mi_data_bytes = 0;
        uint32_t mi_max_count = 0;
    };
```

**改为**：
```cpp
    #include<hgl/mtl/InstanceDataLayout.h>
    // ...
    struct MaterialFinalizePlan
    {
        std::vector<DescriptorSetType> mp_set_types;
        mtl::InstanceDataLayout instance_layout = mtl::InstanceDataLayout::None;
        uint32_t mi_max_count = 0;
    };
```

### C.4 修改 `src/SceneGraph/module/MaterialFinalizeFlowAdapter.cpp`

**当前代码** (L12-13)：
```cpp
        out_plan.mi_data_bytes = mci.GetMaterialInstanceStride();
        out_plan.mi_max_count = mci.GetMaterialInstanceMaxCount();
```

**改为**：
```cpp
        out_plan.instance_layout = ResolveLayoutFromStride(mci.GetMaterialInstanceStride());
        out_plan.mi_max_count    = mci.GetMaterialInstanceMaxCount();
```

**新增辅助函数**（同一文件或 `InstanceDataLayout.h` 中）：
```cpp
inline mtl::InstanceDataLayout ResolveLayoutFromStride(uint32_t stride)
{
    // 短期方案：从 stride 反查枚举（有碰撞风险——Color4f 和 PBRStandard 都是 16B）
    // 长期方案：shader 编译阶段直接输出 InstanceDataLayout 枚举值
    for (uint8_t i = 0; i < uint8_t(mtl::InstanceDataLayout::RANGE_SIZE); ++i)
        if (mtl::kInstanceDataLayouts[i].stride == stride)
            return mtl::InstanceDataLayout(i);
    return mtl::InstanceDataLayout::None;
}
```

> ⚠️ **stride 碰撞警告**：`Color4f`(16B) 和 `PBRStandard`(16B) stride 相同。此辅助函数返回第一个匹配，即 `Color4f`。
> 短期可接受（PBRStandard 的 surface shader 路径不同，不会走到同一个 Domain）。
> 长期方案见 Phase F.2。

### C.5 修改 `src/SceneGraph/module/MaterialManager.cpp` — FinalizePlan 消费处

**当前代码** (L307-308)：
```cpp
    mtl->mi_data_bytes = finalize_plan.mi_data_bytes;
    mtl->mi_max_count  = finalize_plan.mi_max_count;
```

**改为**：
```cpp
    mtl->required_instance_layout = finalize_plan.instance_layout;
    mtl->mi_max_count             = finalize_plan.mi_max_count;
```

### C.6 修改所有 `SetTextureArraySlotFlags` 调用点

以下所有调用点需改名为 `SetRequiredTextureArraySlots`（参数不变）：

| 文件 | 行号 | 当前调用 |
|------|------|---------|
| `src/SceneGraph/module/MaterialManager.cpp` | L689 | `mat->SetTextureArraySlotFlags(flags)` |
| `src/SceneGraph/module/MaterialManager.cpp` | L750 | `mat->SetTextureArraySlotFlags(flags)` |
| `src/ecs/systems/render/QuadResourcePrepareSystem.cpp` | L453 | `dr.material->SetTextureArraySlotFlags(...)` |
| `src/ecs/systems/render/QuadResourcePrepareSystem.cpp` | L479 | `dr.material->SetTextureArraySlotFlags(...)` |

全部改为 → `SetRequiredTextureArraySlots(...)`

### C.7 修改所有 `GetTextureArraySlotFlags` 调用点

以下调用点需改名为 `GetRequiredTextureArraySlots`：

| 文件 | 行号 | 当前调用 | 改为 |
|------|------|---------|------|
| `src/ecs/support/MaterialInstanceAssignmentBuffer.cpp` | L57 | `mtl->GetTextureArraySlotFlags() != 0` | `mtl->GetRequiredTextureArraySlots() != 0` |
| `src/ecs/support/MaterialInstanceAssignmentBuffer.cpp` | L65 | `mtl->GetTextureArraySlotFlags()` (日志) | `mtl->GetRequiredTextureArraySlots()` |
| `src/ecs/support/MaterialInstanceAssignmentBuffer.cpp` | L76 | `mtl->GetTextureArraySlotFlags() != 0` | `mtl->GetRequiredTextureArraySlots() != 0` |
| `src/ecs/support/MaterialInstanceAssignmentBuffer.cpp` | L84 | `mtl->GetTextureArraySlotFlags()` (日志) | `mtl->GetRequiredTextureArraySlots()` |
| `src/ecs/support/PrimitiveBatchPipeline.cpp` | L637 | `material->GetTextureArraySlotFlags() != 0` | `material->GetRequiredTextureArraySlots() != 0` |
| `src/ecs/support/PrimitiveBatchPipeline.cpp` | L647 | `material->GetTextureArraySlotFlags()` (日志) | `material->GetRequiredTextureArraySlots()` |
| `src/ecs/systems/render/QuadMaterialBindingSystem.cpp` | L263 | `dr->material->GetTextureArraySlotFlags()` | `dr->material->GetRequiredTextureArraySlots()` |

### C.8 删除 deprecated 旧重载

此时可安全删除 Phase B 中保留的过渡函数：
- `MaterialManager::CreateMaterialResourceDomain(MaterialTemplate *mtl)` — 不再有调用者
- `MaterialManager::CreateMaterialResourceDomain(uint32_t, uint32_t)` — 签名已更新

### C.9 更新 `GetOrCreateDefaultDomain`

```cpp
MaterialResourceDomain *MaterialManager::GetOrCreateDefaultDomain(MaterialTemplate *mtl)
{
    if (!mtl || !mtl->hasMI()) return nullptr;
    auto it = default_domain_map.find(mtl);
    if (it != default_domain_map.end()) return it->second;
    MaterialResourceDomain *domain = CreateMaterialResourceDomain(
        mtl->GetRequiredLayout(), mtl->GetMIMaxCount(), 0);
    default_domain_map[mtl] = domain;
    return domain;
}
```

### C.10 验证

- [ ] 编译通过
- [ ] `MaterialTemplate.h` 不再持有 `mi_data_bytes` 成员
- [ ] `GetMIDataBytes()` 返回值与之前一致
- [ ] `SetTextureArraySlotFlags` / `GetTextureArraySlotFlags` 零残留（全文搜索确认）
- [ ] 跑 example，行为不变

---

## Phase D — DomainMaterialBinding 校验升级

**目标**：DMB 创建时使用语义级双重校验（enum 匹配 + bitmask 子集检查），替代当前的 byte-stride 比较。

### D.1 修改 `src/SceneGraph/module/MaterialManager.cpp` — `CreateDomainMaterialBinding`

**当前代码** (L960-967)：
```cpp
    // Hard reject: MI stride mismatch means MI data cannot be shared
    if (domain->GetMIDataBytes() != mtl->GetMIDataBytes())
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateDomainMaterialBinding: MI stride mismatch "
            "domain=%u mtl=%u\n",
            domain->GetMIDataBytes(), mtl->GetMIDataBytes());
        return nullptr;
    }
```

**改为**：
```cpp
    // 语义级校验 ①：InstanceDataLayout 枚举一致
    if (domain->GetLayout() != mtl->GetRequiredLayout())
    {
        std::fprintf(stderr,
            "[MaterialManager] CreateDomainMaterialBinding: layout mismatch "
            "domain=%s mtl=%s\n",
            mtl::GetInstanceDataName(domain->GetLayout()),
            mtl::GetInstanceDataName(mtl->GetRequiredLayout()));
        return nullptr;
    }

    // 语义级校验 ②：TextureArray slot 供需满足
    {
        const uint8_t required  = mtl->GetRequiredTextureArraySlots();
        const uint8_t available = domain->GetTextureArraySlots();
        if ((required & available) != required)
        {
            std::fprintf(stderr,
                "[MaterialManager] CreateDomainMaterialBinding: texture slot mismatch "
                "required=0x%02X available=0x%02X\n",
                required, available);
            return nullptr;
        }
    }
```

### D.2 验证

- [ ] 编译通过
- [ ] 现有所有 DMB 创建路径仍能成功（enum 匹配 + slots 满足）
- [ ] 故意传入不匹配 layout 的 Domain + Material，确认被拒绝并打印正确日志

---

## Phase E — MIT 来源切换（供方驱动）

**目标**：`Primitive::InitMITLayout` 的 slot flags 来源从 `material->GetTextureArraySlotFlags()` 改为 `domain->GetTextureArraySlots()`。MIT packed 数组长度由 Domain 全集决定。

### E.1 修改 `inc/hgl/graph/PrimitiveMaterialSlot.h`

**当前代码** (L37)：
```cpp
    uint8_t  texture_array_slot_flags = 0;
```

**改为**（语义不变，来源从 Material 改为 Domain）：
```cpp
    uint8_t  texture_array_slot_flags = 0;  // 来源：domain->GetTextureArraySlots()
```

> 字段名不改（PrimitiveMaterialSlot 是 POD 传输体，字段名是其自身语义，不体现来源）。

### E.2 修改 `AllocMaterialInstanceSlot`（MaterialManager.cpp L902-928）

**当前代码**未填充 `slot.texture_array_slot_flags`。需要在此处补上：

```cpp
    slot.texture_array_slot_flags = domain->GetTextureArraySlots();  // 供方决定
```

加在 `slot.preset = preset;` 之后。

### E.3 确认 Primitive::InitMITLayout 调用源

**当前代码** (Primitive.cpp L136, L164)：
```cpp
    InitMITLayout(slot.texture_array_slot_flags);
```

这些调用已经读取 slot 的 flags，而 slot 在 E.2 中已改为从 Domain 获取。**无需修改 Primitive 代码**。

### E.4 修改 `QuadResourcePrepareSystem.cpp` 中的 TextureArray slot 设置

**当前代码** (L453, L479) 直接设置到 Material 上：
```cpp
    dr.material->SetTextureArraySlotFlags(
        uint8_t(1u << uint8_t(graph::mtl::SamplerSlot::BaseColor)));
```

**改为**：设到 Domain 上（前提是此时 Domain 已创建）。

```cpp
    // Material 侧：声明需求
    dr.material->SetRequiredTextureArraySlots(
        uint8_t(1u << uint8_t(graph::mtl::SamplerSlot::BaseColor)));
    // Domain 侧：提供资源（如果此处也创建了 Domain）
    if (dr.dmb && dr.dmb->GetDomain())
        // Domain 在创建时已设置 tex_array_slots，无需二次设置
```

> 如果 QuadResourcePrepareSystem 中的 Domain 是通过 `MaterialAssetRegistry::Acquire` 创建的，需确认 Acquire 内部已将 `tex_array_slots` 传给 `CreateMaterialResourceDomain`。

### E.5 修改 `QuadMaterialBindingSystem.cpp` (L263)

**当前代码**：
```cpp
    const uint8_t texture_array_slot_flags = dr->material->GetTextureArraySlotFlags();
```

**改为**：
```cpp
    const uint8_t texture_array_slot_flags = dr->domain
        ? dr->domain->GetTextureArraySlots()
        : dr->material->GetRequiredTextureArraySlots();  // fallback
```

> 优先从 Domain 读（供方），fallback 到 Material（兼容过渡期）。

### E.6 修改 `PrimitiveBatchPipeline.cpp` (L637, L647)

**当前代码**：
```cpp
    if (primitive && material->GetTextureArraySlotFlags() != 0 && ...)
    // ...
    unsigned(material->GetTextureArraySlotFlags()),
```

**改为**：
```cpp
    const uint8_t batch_slot_flags = domain ? domain->GetTextureArraySlots()
                                            : material->GetRequiredTextureArraySlots();
    if (primitive && batch_slot_flags != 0 && primitive->GetMITDataBytes() == 0)
    // ...
    unsigned(batch_slot_flags),
```

### E.7 修改 `MaterialInstanceAssignmentBuffer.cpp` (L57, L65, L76, L84)

类似 E.6，TextureArray 相关判断从 `mtl->GetRequiredTextureArraySlots()` 改为优先从 `domain->GetTextureArraySlots()` 读取。需要在 `BindMaterialInstanceTextureID` 函数中增加 Domain 参数或通过上下文获取。

> **选项 A**（推荐）：MaterialInstanceAssignmentBuffer 已有 domain 成员/可访问 → 直接改。
> **选项 B**：暂时保持从 Material 读取（需方声明在此处也是正确的——"本批次是否需要处理 TextureArray"）。

⚠️ 具体方案视 `MaterialInstanceAssignmentBuffer` 是否持有 Domain 引用而定。若无，则先采用选项 B，后续再统一。

### E.8 验证

- [ ] 编译通过
- [ ] MIT packed 数组长度 = popcount(domain->GetTextureArraySlots())
- [ ] Billboard/Quad 系统仍正确渲染
- [ ] 同 Domain 下切换 Material 变种，MIT 无需重分配

---

## Phase F — Shader 层整理（可独立排期）

**目标**：各 surface shader 的 inline `struct MaterialInstance` 改为 `#include` 引用 Phase A 创建的权威 GLSL 文件。

### F.1 Shader 文件清单

需修改的 surface shader（inline 定义 → `#include`）：

| 文件 | 当前 struct | 改为 |
|------|-------------|------|
| `ShaderLibrary/surface/gizmo3d_surface.glsl` | `struct MaterialInstance { vec4 color; }` | `#include "instance_data/Color4f.glsl"` |
| `ShaderLibrary/surface/unlit_color3d_surface.glsl` | `struct MaterialInstance { vec4 color; }` | `#include "instance_data/Color4f.glsl"` |
| `ShaderLibrary/surface/unlit_luminance_surface.glsl` | `struct MaterialInstance { vec4 color; }` | `#include "instance_data/Color4f.glsl"` |
| `ShaderLibrary/surface/pbrcolor3d_surface.glsl` | `struct MaterialInstance { uint base_color; float metallic; float roughness; }` | `#include "instance_data/PBRColor.glsl"` |
| `ShaderLibrary/surface/standard3d_surface.glsl` | `struct MaterialInstance { uint base_color; float metallic; float roughness; float normal_scale; }` | `#include "instance_data/PBRStandard.glsl"` |
| `ShaderLibrary/surface/textureblinnphong3d_surface.glsl` | `struct MaterialInstance { float normal_strength; }` | `#include "instance_data/TextureBlinnPhong.glsl"` |

### F.2 长期方案：消除 stride 碰撞

在 shader 编译入口增加 `#pragma instance_data_layout Color4f` 标注，让 `MaterialCreateInfo` 直接输出 `InstanceDataLayout` 枚举值而非仅 stride。这样 `ResolveLayoutFromStride()` 辅助函数可以彻底删除。

**涉及文件**：
- `inc/hgl/shadergen/MaterialCreateInfo.h`：新增 `InstanceDataLayout` 字段
- Shader 编译器前端：解析 `#pragma instance_data_layout`
- `MaterialFinalizeFlowAdapter.cpp`：直接读取 enum 而非 stride 反查

> 此步骤复杂度较高，建议作为后续独立 PR。

### F.3 验证

- [ ] 所有 shader 编译通过
- [ ] `struct MaterialInstance` 定义只在 `ShaderLibrary/instance_data/*.glsl` 中出现
- [ ] 全量渲染回归测试通过

---

## 依赖关系总图

```
Phase A ─────────────────┐
(enum + GLSL files)      │
                         │
Phase B ─────────────────┤
(Domain 内部改造)          │
         ↓               │
Phase C ─────────────────┤
(Template + 所有调用点)    │
         ↓               │
Phase D ─────────────────┤
(DMB 校验升级)             │
         ↓               │
Phase E ─────────────────┘
(MIT 供方驱动)

Phase F（独立分支，任何时候均可开始）
(Shader #include 整理)
```

**严格顺序**：A → B → C → D → E
**独立**：F 可在 A 完成后任何时候进行

---

## 文件变更汇总

### 新建文件 (Phase A)

| 文件 | 说明 |
|------|------|
| `inc/hgl/mtl/InstanceDataLayout.h` | enum + constexpr 注册表 |
| `ShaderLibrary/instance_data/Color4f.glsl` | 权威 GLSL struct (16B) |
| `ShaderLibrary/instance_data/PBRColor.glsl` | 权威 GLSL struct (12B) |
| `ShaderLibrary/instance_data/PBRStandard.glsl` | 权威 GLSL struct (16B) |
| `ShaderLibrary/instance_data/TextureBlinnPhong.glsl` | 权威 GLSL struct (4B) |

### 修改文件

| 文件 | Phase | 主要变更 |
|------|-------|---------|
| `inc/hgl/vk/VKMaterialResourceDomain.h` | B | `mi_data_bytes` → `instance_layout` enum；新增 `texture_array_slot_flags`；删除 `MaterialTemplate*` 构造函数 |
| `src/SceneGraph/VKMaterialResourceDomain.cpp` | B | 构造函数参数改为 (enum, count, slots)；删除委托构造函数；删除 `#include VKMaterialTemplate.h` |
| `inc/hgl/vk/VKMaterialTemplate.h` | C | `mi_data_bytes` → `required_instance_layout`；`texture_array_slot_flags` → `required_texture_array_slots`；方法改名 |
| `src/SceneGraph/VKMaterialTemplate.cpp` | C | 构造函数初始化改为 enum |
| `inc/hgl/graph/module/MaterialFinalizeFlowAdapter.h` | C | 结构体字段 `mi_data_bytes` → `instance_layout` |
| `src/SceneGraph/module/MaterialFinalizeFlowAdapter.cpp` | C | stride → layout 查表 |
| `inc/hgl/graph/module/MaterialManager.h` | B/C | `CreateMaterialResourceDomain` 签名更新 |
| `src/SceneGraph/module/MaterialManager.cpp` | B/C/D | Domain 创建、DMB 校验、FinalizePlan 消费、TextureArraySlotFlags 调用点 |
| `src/ecs/support/MaterialInstanceAssignmentBuffer.cpp` | C/E | `GetTextureArraySlotFlags` → `GetRequiredTextureArraySlots` 或 Domain 读取 |
| `src/ecs/support/PrimitiveBatchPipeline.cpp` | C/E | `GetTextureArraySlotFlags` → `GetRequiredTextureArraySlots` 或 Domain 读取 |
| `src/ecs/systems/render/QuadResourcePrepareSystem.cpp` | C/E | `SetTextureArraySlotFlags` → `SetRequiredTextureArraySlots` + Domain 侧供给 |
| `src/ecs/systems/render/QuadMaterialBindingSystem.cpp` | C/E | `GetTextureArraySlotFlags` → Domain 读取 |
| `src/SceneGraph/mesh/Primitive.cpp` | — | `InitMITLayout` 已通过 slot 间接改变，无需直接修改 |
| `inc/hgl/graph/PrimitiveMaterialSlot.h` | E | 注释更新（来源改为 Domain） |
| 6 个 surface `.glsl` | F | inline struct → `#include "instance_data/XXX.glsl"` |

---

## 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| stride 碰撞（Color4f=PBRStandard=16B） | `ResolveLayoutFromStride()` 可能返回错误 enum | 短期：surface 类型路径不同，不会混用。长期：Phase F.2 `#pragma` 标注 |
| TextureArray 迁移到 Domain 后，QuadResourcePrepareSystem 创建 Domain 时机 | 可能在 `SetRequiredTextureArraySlots` 之前创建了 Domain | 确认调用顺序：先设置 Material 需求，再创建 Domain（Domain 创建需传入 `tex_array_slots`） |
| `GetMIDataBytes()` 接口保持不变但内部查表 | 理论上返回值相同 | Phase B 验证：遍历所有 Domain/Material，断言新旧值一致 |
| 多线程 `SetRequiredTextureArraySlots` | 与 Domain 创建竞态 | 仅在初始化/创建阶段设置，渲染帧内只读——已有保证 |

---

## 检查清单（每 Phase 完成后）

- [ ] 全量编译通过
- [ ] 全文搜索已删除 API（`mi_data_bytes`, `SetTextureArraySlotFlags`, `GetTextureArraySlotFlags`）确认零残留
- [ ] 至少一个 example（如 `05_DomeSkyMinimal`）正确渲染
- [ ] Billboard/Quad 文字系统正确渲染
- [ ] stderr 无新增 `[MaterialManager]` 错误日志
- [ ] MI 数据正确传递到 GPU（可通过 RenderDoc 抓帧验证 SSBO 内容）
