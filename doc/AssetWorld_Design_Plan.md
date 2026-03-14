# AssetWorld 设计与实施计划

> 文档版本：1.0  
> 日期：2026-03-10  
> 状态：设计阶段，待实施

---

## 1. 背景与动机

### 1.1 问题来源

原有设计试图用 `SubWorldComponent`（执行隔离容器）来承载 `StaticMesh`（多 Primitive 渲染资产）。这导致以下问题：

1. **语义错配**：`SubWorld` 解决"在哪个执行环境运行"，`StaticMesh` 解决"画什么"，两者是不同层次的概念。
2. **资产定义与场景实例无法分离**：100 个 House 实例需要 100 份独立的 SubWorld，相同的 Primitive/Geometry/MaterialInstance 结构被重复持有。
3. **破坏合批**：IndirectDraw + VDM 的终极目标要求同类实体统一流入根 `ECSContext` 的 `PrimitiveRenderPipeline`，而每个 SubWorld 是独立的执行单元，阻断合并路径。
4. **调度开销**：`WorldScheduler::FlatWorldRecord` 为少量场景级 World 设计，数千个 StaticMesh 实例会使调度器 DFS 展开急剧膨胀。

### 1.2 已完成的清理

- 删除 `inc/hgl/ecs/components/StaticMesh.h`（`hgl::ecs::StaticMesh`，未完成的 Node 树）
- 删除 `inc/hgl/ecs/components/StaticMeshComponent.h`（`BuildEntities()`/`spawned_entities` 模式）
- 对应 `src/ecs/components/StaticMesh.cpp` / `StaticMeshComponent.cpp` 已删除
- `CMakeLists.txt`（`ECS_SYS_16`）中对上述文件的引用已清除

保留的 `inc/hgl/graph/mesh/StaticMesh.h`（`hgl::graph::StaticMesh`）是正确的**渲染资产内容体**，不动。

---

## 2. 整体分层架构

```
┌─────────────────────────────────────────────────────────────────┐
│  Application                                                    │
│  ├── GraphicsContext          ← 图形资源层（已有，不大改）        │
│  │     ├── GeometryManager   ← Geometry 去重（path → ptr）      │
│  │     ├── MaterialManager   ← Material 去重                    │
│  │     ├── PrimitiveManager  ← Primitive 去重                   │
│  │     └── ...                                                  │
│  │                                                              │
│  ├── AssetWorldRegistry       ← 资产定义层（新增，轻量）          │
│  │     └── AssetWorldDef[]    → 持有 graph::StaticMesh*         │
│  │                               └── 引用 Geometry/MI（来自上方 Managers）│
│  │                                                              │
│  └── World                                                      │
│        └── ECSContext         ← 执行层（小改：注入 registry 指针）│
│              ├── PrimitiveCollectSystem    （已有）              │
│              ├── AssetInstanceCollectSystem（新增）              │
│              └── Entity                                         │
│                    ├── TransformComponent                       │
│                    └── AssetInstanceComponent  （新增）          │
└─────────────────────────────────────────────────────────────────┘
```

**关键原则：**
- `GraphicsContext` 管理 GPU 资源生命周期，负责资产的**物理去重**（同路径 Geometry/Material 在 Manager 内是同一个指针）。
- `AssetWorldRegistry` 管理**内容定义去重**（同名资产 `AssetWorldDef` 只注册一次），持有 `graph::StaticMesh*` 的非拥有引用。
- `ECS` 层通过 `AssetInstanceComponent` 在场景中放置任意多份实例，只存 `AssetWorldDef::ID` 和实例参数差异，**不重复持有 Primitive 数据**。
- `AssetInstanceCollectSystem` 在 `RenderCollect` 阶段把每个实例的所有 Primitive 推入 `RenderFrameCache`，与普通 `PrimitiveComponent` 走完全相同的后续 Batch → Upload → Draw 管线，**天然合批**。

---

## 3. 核心数据结构定义

### 3.1 AssetWorldDef（资产定义）

**文件**：`inc/hgl/graph/asset/AssetWorldDef.h`

```cpp
namespace hgl::graph
{
    /**
     * AssetWorldDef — 渲染资产的内容定义（不可变，定义注册后只读）
     *
     * 不拥有 StaticMesh，StaticMesh 由 GraphicsContext 各 Manager 管理生命周期。
     */
    struct AssetWorldDef
    {
        using ID = uint32_t;
        static constexpr ID INVALID_ID = 0;

        ID                      id         = INVALID_ID;
        std::string             name;                    // 资产注册名（唯一）
        graph::StaticMesh*      mesh       = nullptr;    // 内容体（非拥有）
        BoundingVolumes         bounds;                  // 所有 Primitive 合并包围体（缓存）

        bool IsValid() const { return id != INVALID_ID && mesh != nullptr; }
    };
}
```

**设计说明：**
- `StaticMesh*` 非拥有，生命周期由外部（Application / 资产加载器）管理，在 Registry 注销前必须有效。
- `bounds` 由注册时从 `mesh->GetBoundingVolumes()` 拷贝缓存，避免每帧访问。
- `ID` 是 `uint32_t` 值类型，可安全存储在 Component 里或序列化。

---

### 3.2 AssetWorldRegistry（资产注册表）

**文件**：`inc/hgl/graph/asset/AssetWorldRegistry.h`  
**实现**：`src/SceneGraph/asset/AssetWorldRegistry.cpp`

```cpp
namespace hgl::graph
{
    class AssetWorldRegistry
    {
    public:
        using ID = AssetWorldDef::ID;

    private:
        std::unordered_map<std::string, ID>   name_to_id;
        std::vector<AssetWorldDef>             defs;         // 下标 = ID-1（ID 从 1 开始）
        ID                                     next_id = 1;

    public:
        /**
         * 注册一个资产定义。
         * @param name   资产唯一名称（重复注册返回已有 ID，覆盖 mesh 指针）
         * @param mesh   指向已构建的 graph::StaticMesh（调用方保证生命周期）
         * @return       分配的 AssetWorldDef::ID
         */
        ID               Register(const std::string& name, graph::StaticMesh* mesh);

        /**
         * 注销资产定义（不销毁 mesh，由调用方管理）。
         */
        void             Unregister(ID id);
        void             Unregister(const std::string& name);

        /**
         * 查询。
         */
        const AssetWorldDef* Get(ID id)            const;
        const AssetWorldDef* Find(const std::string& name) const;
        ID                   FindID(const std::string& name) const;

        /**
         * 枚举所有已注册资产（用于编辑器、序列化）。
         */
        const std::vector<AssetWorldDef>& GetAll() const { return defs; }

        size_t Count() const { return name_to_id.size(); }
        void   Clear();
    };
}
```

**归属**：
- `AssetWorldRegistry` 由 **Application 级**持有（不放进 `GraphicsContext`，因为它是场景内容定义，不是 GPU 资源）。
- `GraphicsContext` 中的 Manager 负责 GPU 资源去重，`AssetWorldRegistry` 负责内容定义去重，两者正交。

---

### 3.3 AssetInstanceComponent（场景实例组件）

**文件**：`inc/hgl/ecs/components/AssetInstanceComponent.h`  
**实现**：`src/ecs/components/AssetInstanceComponent.cpp`

```cpp
namespace hgl::ecs
{
    /**
     * AssetInstanceComponent — 场景中对资产定义的一个实例引用
     *
     * 与 TransformComponent 配合，在场景中放置一个 AssetWorldDef 实例。
     * 不生成子 Entity，由 AssetInstanceCollectSystem 在 RenderCollect 阶段
     * 将资产所有 Primitive 推入 RenderFrameCache，和 PrimitiveComponent 共享
     * 后续全部 Batch/Draw 管线。
     */
    class AssetInstanceComponent : public Component
    {
    public:
        using AssetID = graph::AssetWorldDef::ID;

    private:
        AssetID     asset_id   = graph::AssetWorldDef::INVALID_ID;
        bool        visible    = true;

        // 逐实例可选覆盖（扩展用，初始为空）
        // uint32_t  lod_level  = 0;
        // float     lod_bias   = 0.0f;

    public:
        explicit AssetInstanceComponent(const std::string& name = "AssetInstance")
            : Component(name) {}

        ~AssetInstanceComponent() override = default;

    public:
        const char* GetSystemGroupName() const override { return "AssetInstance"; }

        void    SetAssetID(AssetID id);
        AssetID GetAssetID() const { return asset_id; }

        void    SetVisible(bool v);
        bool    IsVisible()  const { return visible; }

        bool    IsValid()    const { return asset_id != graph::AssetWorldDef::INVALID_ID; }

    public:
        void OnAttach() override;
        void OnDetach() override;
    };
}
```

**设计要点：**
- `asset_id` 是 POD 值，可直接序列化（存资产名 → 加载时通过 Registry 解析为 ID）。
- `AssetInstanceComponent` 通过 `GetSystemGroupName()` 返回 `"AssetInstance"` 触发 ECS 自动安装 `AssetInstanceCollectSystem`（同 `PrimitiveComponent` 的 `"Primitive"` 机制完全一致）。

---

### 3.4 AssetInstanceCollectSystem（渲染收集系统）

**文件**：`inc/hgl/ecs/systems/render/AssetInstanceCollectSystem.h`  
**实现**：`src/ecs/systems/render/AssetInstanceCollectSystem.cpp`

```cpp
namespace hgl::ecs
{
    /**
     * AssetInstanceCollectSystem — 将 AssetInstanceComponent 展开为 RenderItem
     *
     * 执行阶段：RenderCollect（与 RenderPrimitiveCollectSystem 同阶段）
     *
     * 对每个带 AssetInstanceComponent + TransformComponent 的可见实体：
     *   获取 AssetWorldDef → 遍历其 StaticMesh::primitive_list
     *   → 对每个 Primitive 构建 PrimitiveRenderItem
     *   → 推入 RenderFrameCache::render_items
     *
     * 后续 PrimitiveCullSystem / PrimitiveSortSystem / PrimitiveBuildSystem /
     * PrimitiveRenderSystem 对此输出完全透明，无需感知来源是 AssetInstance。
     */
    class AssetInstanceCollectSystem : public System
    {
    private:
        EntityQuery* query = nullptr;
        graph::AssetWorldRegistry* asset_registry = nullptr;  // 非拥有

    public:
        explicit AssetInstanceCollectSystem(ECSContext* ctx);

        void SetAssetRegistry(graph::AssetWorldRegistry* reg) { asset_registry = reg; }

        void Update(float dt) override {}     // 无 Tick 逻辑
        void Render(RenderCmdBuffer* cmd, float dt) override;   // 阶段：RenderCollect
    };
}
```

**展开逻辑（伪代码）：**

```cpp
void AssetInstanceCollectSystem::Render(RenderCmdBuffer*, float)
{
    if (!asset_registry) return;

    for (EntityID eid : query->GetEntities())
    {
        auto* entity      = context->GetEntity(eid);
        auto* inst        = entity->GetComponent<AssetInstanceComponent>();
        auto* transform   = entity->GetComponent<TransformComponent>();

        if (!inst->IsValid() || !inst->IsVisible()) continue;

        const AssetWorldDef* def = asset_registry->Get(inst->GetAssetID());
        if (!def || !def->IsValid())            continue;

        // 视锥剔除（用 def->bounds 变换后与视锥测试）
        if (!IsInFrustum(def->bounds, transform->GetWorldMatrix())) continue;

        // 展开：每个 Primitive 生成一个 PrimitiveRenderItem
        for (Primitive* prim : def->mesh->GetPrimitiveList())
        {
            auto item = MakePrimitiveRenderItem(eid, entity, transform, prim);
            context->GetRenderFrameCache().AddRenderItem(std::move(item));
        }
    }
}
```

---

## 4. ECSContext 改动（最小化）

`ECSContext` 只需新增一个非拥有指针：

```cpp
// inc/hgl/ecs/core/Context.h 新增字段
graph::AssetWorldRegistry* asset_registry = nullptr;

// 对应 getter/setter
void SetAssetRegistry(graph::AssetWorldRegistry* reg) { asset_registry = reg; }
graph::AssetWorldRegistry* GetAssetRegistry() const   { return asset_registry; }
```

注入时机：Application 初始化，在 `InitializeGraphics()` 之后：

```cpp
// Application::Initialize() 伪代码
graphics_context  = new GraphicsContext(device);
asset_registry    = new AssetWorldRegistry();
world->GetContext()->SetAssetRegistry(asset_registry);
```

**不需要改动 `World` 或 `WorldScheduler`**。

---

## 5. `graph::StaticMesh`（保留，微调）

`inc/hgl/graph/mesh/StaticMesh.h`（`hgl::graph::StaticMesh`）原样保留，它就是 `AssetWorldDef` 的内容体。

唯一可选的微调：`StaticMesh` 目前提供 `PrimitiveList`（`ManagedArray<Primitive>`）。
如果将来需要 Node 层级（骨架/LOD 组），可在 `StaticMesh` 内部扩展，对外接口不变。  
**本期不做**。

---

## 6. 分阶段实施计划

### Phase A — 资产定义层（无 ECS 依赖，可独立编译测试）

| 步骤 | 文件 | 工作内容 |
|------|------|---------|
| A1 | `inc/hgl/graph/asset/AssetWorldDef.h` | 定义 `AssetWorldDef` 结构体 |
| A2 | `inc/hgl/graph/asset/AssetWorldRegistry.h` | 声明 `AssetWorldRegistry` 类 |
| A3 | `src/SceneGraph/asset/AssetWorldRegistry.cpp` | 实现 Register / Unregister / Get / Find |
| A4 | `CMakeModule/CMGUI` 或 `CMSceneGraph/CMakeLists.txt` | 将 A3 加入编译 |
| A5 | 烟雾测试 | 注册两个资产、Find by name、Unregister、重查返回 null — 纯 CPU 单元测试 |

### Phase B — ECS 组件与系统

| 步骤 | 文件 | 工作内容 |
|------|------|---------|
| B1 | `inc/hgl/ecs/components/AssetInstanceComponent.h` | 声明组件类 |
| B2 | `src/ecs/components/AssetInstanceComponent.cpp` | 实现 OnAttach / OnDetach / SetAssetID |
| B3 | `inc/hgl/ecs/systems/render/AssetInstanceCollectSystem.h` | 声明系统类 |
| B4 | `src/ecs/systems/render/AssetInstanceCollectSystem.cpp` | 实现 Render()（Collect 阶段展开逻辑） |
| B5 | `inc/hgl/ecs/core/Context.h` + `Context.cpp` | 添加 `asset_registry*` 字段、getter/setter |
| B6 | `src/ecs/CMakeLists.txt` | 将 B1-B4 加入 `ECS_COMPONENT_FILES` / `ECS_SYS_16` |
| B7 | SystemGroup 注册 | 在 `DefaultSystems.cpp` 的 `InstallPrimitiveGroup` 后添加 `InstallAssetInstanceGroup` |

### Phase C — 视锥剔除与 LOD 支持

| 步骤 | 工作内容 |
|------|---------|
| C1 | `AssetInstanceCollectSystem` 使用 `def->bounds` 进行基础 AABB 视锥测试 |
| C2 | `BoundingBoxComponent` 同步 — 若 Entity 带 `BoundingBoxComponent`，以资产包围体为基础更新（可选） |
| C3 | LOD 字段扩展：`AssetWorldDef` 增加 `lod_meshes[4]`，`AssetInstanceComponent` 增加 `lod_level` 覆盖 |

### Phase D — 序列化支持

| 步骤 | 工作内容 |
|------|---------|
| D1 | `ComponentRecords.h`：添加 `AssetInstanceRecord`（持 `asset_name: string`，反序列化时通过 Registry 解析 ID） |
| D2 | `ContextSerialization.cpp`：注册 `AssetInstanceComponent` 序列化器 |
| D3 | `AssetWorldRegistry` 持久化：提供 `ToJSON / FromJSON`，保存注册表快照 |

---

## 7. 文件清单总览

### 新增文件

```
inc/hgl/graph/asset/
  AssetWorldDef.h                           ← 资产定义结构体（Phase A1）
  AssetWorldRegistry.h                      ← 注册表声明（Phase A2）

src/SceneGraph/asset/
  AssetWorldRegistry.cpp                    ← 注册表实现（Phase A3）

inc/hgl/ecs/components/
  AssetInstanceComponent.h                  ← ECS 组件（Phase B1）

src/ecs/components/
  AssetInstanceComponent.cpp                ← 组件实现（Phase B2）

inc/hgl/ecs/systems/render/
  AssetInstanceCollectSystem.h              ← 收集系统（Phase B3）

src/ecs/systems/render/
  AssetInstanceCollectSystem.cpp            ← 系统实现（Phase B4）
```

### 修改文件

```
inc/hgl/ecs/core/Context.h                  ← 新增 asset_registry* 字段（Phase B5）
src/ecs/core/Context.cpp                    ← 初始化/Shutdown 时清空指针（Phase B5）
src/ecs/CMakeLists.txt                      ← 加入新文件（Phase B6）
src/ecs/core/DefaultSystems.cpp             ← 注册 AssetInstance SystemGroup（Phase B7）
```

### 已删除文件（已完成）

```
inc/hgl/ecs/components/StaticMesh.h         ✓ 已删除
inc/hgl/ecs/components/StaticMeshComponent.h✓ 已删除
src/ecs/components/StaticMesh.cpp           ✓ 已删除
src/ecs/components/StaticMeshComponent.cpp  ✓ 已删除
```

### 保留不动

```
inc/hgl/graph/mesh/StaticMesh.h             ← graph::StaticMesh，资产内容体，原样保留
src/SceneGraph/mesh/StaticMesh.cpp
inc/hgl/graph/core/GraphicsContext.h        ← 资源层，原样保留
```

---

## 8. 数据流全景（实施完成后）

```
资产加载时（一次性）：
  LoadStaticMesh(path)
    └─► GraphicsContext::GeometryManager::Load(path)   → Geometry*  (GPU 去重)
    └─► GraphicsContext::PrimitiveManager::Create(...)  → Primitive* (GPU 去重)
    └─► graph::StaticMesh::AddPrimitive(prim)
    └─► AssetWorldRegistry::Register("House", &mesh)   → AssetID = 1

场景编辑时（N 次）：
  Entity* e = world->CreateEntity("House_42");
  e->AddComponent<TransformComponent>()->SetPosition({10,0,5});
  e->AddComponent<AssetInstanceComponent>()->SetAssetID(1);  // 只存 ID

每帧 RenderCollect 阶段：
  AssetInstanceCollectSystem::Render()
    └─► 遍历所有 AssetInstanceComponent 实体
    └─► Get(asset_id=1) → AssetWorldDef → StaticMesh → PrimitiveList[0..N]
    └─► 对每个 Primitive → MakePrimitiveRenderItem(entity_transform, prim)
    └─► 推入 RenderFrameCache

后续管线（无感知来源）：
  PrimitiveCullSystem    → 视锥剔除
  PrimitiveSortSystem    → Material/Pipeline 排序
  PrimitiveBuildSystem   → 构建 VAB（TransformAssignmentBuffer + MaterialInstanceAssignmentBuffer）
  PrimitiveBuildSystem   → 写 Indirect Draw Buffer
  PrimitiveRenderSystem  → vkCmdDrawIndexedIndirect
                         = 所有 House 实例 + 普通 Primitive 实体 → 单 DrawCall ✓
```

---

## 9. 与现有系统的兼容性

| 现有机制 | 兼容性 | 说明 |
|---------|--------|------|
| `PrimitiveComponent` 单实体 | ✅ 完全不变 | 继续走原有 `RenderPrimitiveCollectSystem` |
| `SubWorldComponent` | ✅ 完全不变 | 用于真正需要独立执行逻辑的子世界（AI/物理隔离等） |
| `TransformAssignmentBuffer` | ✅ 透明复用 | AssetInstance 展开后每个 RenderItem 仍带 transform_id |
| `MaterialInstanceAssignmentBuffer` | ✅ 透明复用 | 同上 |
| `VDM (VertexDataManager)` | ✅ 透明复用 | Geometry 通过 GeometryManager 已共享 VDM 块 |
| `BillboardRenderPipelineGroup` | ✅ 不受影响 | 走独立路径 |
| 序列化（`ContextSerialization`） | 🔧 Phase D 补充 | 需增加 `AssetInstanceComponent` 序列化器 |
| `ECSContext::graphics_context*` | ✅ 不变 | AssetInstance 不新增 GPU 资源创建入口 |

---

## 10. 不做的事（有意排除）

| 排除项 | 原因 |
|--------|------|
| `AssetWorldRegistry` 放入 `GraphicsContext` | 内容定义与 GPU 资源管理职责正交；registry 可以在 device 初始化前就存在 |
| `AssetWorldDef` 内含 ECSContext | 定义层不依赖执行层，保持单向依赖 |
| `AssetInstanceComponent::BuildEntities()` | 这是原 `StaticMeshComponent` 被删除的根本原因，不重复 |
| SkeletonMesh/动画 | 超出当前 Phase 范围，后续独立设计 |
| Node 层级（多根变换） | `graph::StaticMesh` 可以后续扩展，本期不需要 |
