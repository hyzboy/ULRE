# ECS 核心组件详解

本文档梳理 ULRE ECS 框架中 **Object / Component / Entity / EntityManager / EntityQuery / System / ECSContext / World / WorldScheduler / SubWorldComponent** 各核心类的成员、职责及相互协作关系。

---

## 1. Object —— 所有 ECS 对象的公共基类

文件：`inc/hgl/ecs/core/Object.h`

```cpp
class Object
{
public:
    using ObjectID = uint64_t;
    static constexpr ObjectID INVALID_OBJECT_ID = 0;

protected:
    ObjectID objectId;      // 全局自增 ID，进程内唯一
    std::string objectName;

public:
    ObjectID    GetID()   const { return objectId; }
    const std::string& GetName() const { return objectName; }
    bool        IsValid() const { return objectId != INVALID_OBJECT_ID; }

    // 生命周期钩子 (可重写)
    virtual void OnCreate()       {}
    virtual void OnUpdate(float)  {}
    virtual void OnDestroy()      {}
};
```

`ECSContext`、`Entity`、`System`、`Component` 全部继承自 `Object`，因此任何对象都可通过 `GetID()` / `GetName()` 唯一标识。

---

## 2. Component —— 数据与行为的最小单元

文件：`inc/hgl/ecs/core/Component.h`

### 2.1 关键字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `version` | `uint64_t` | 每次数据变动时自增，用于脏检测 |
| `change_mask` | `uint32_t` | 位域，标记哪些字段发生了变化 |
| `owner_id` | `EntityID` | 所属 Entity 的 ID |
| `owner_context` | `ECSContext*` | 非拥有指针，指向宿主 Context |

### 2.2 生命周期

```
AddComponent<T>() ──► OnAttach()
每帧逻辑更新      ──► [OnUpdate()]
RemoveComponent() ──► OnDetach()
```

### 2.3 变更通知

```cpp
void TouchChange(uint32_t mask)
{
    ++version;
    change_mask |= mask;
}
```

系统可读取 `version` 判断是否需要重新处理该组件，避免无谓计算。

### 2.4 系统组自动激活

```cpp
virtual std::string GetSystemGroupName() const { return ""; }
```

当组件被挂载到 Entity 时，ECSContext 会调用 `GetSystemGroupName()`，若返回非空字符串则自动触发对应 SystemGroup 的安装（via `EnsureSystemGroupSystems`），实现"组件即配置"的自驱动架构。

---

## 3. Entity —— 组件的运行时容器

文件：`inc/hgl/ecs/core/Entity.h`

### 3.1 核心结构

```cpp
class Entity : public Object
{
    UnorderedMap<size_t, shared_ptr<Component>> components;
    // key = typeid(T).hash_code()

    EntityID id;           // 含世代号的句柄
    ECSContext* context;   // 非拥有指针
};
```

### 3.2 组件操作流程

**AddComponent\<T\>()**
```
new T()
  └─► ReplaceComponent(hash, ptr)
        └─► RegisterToContext(ptr)     // 写入 component_registry
              └─► NotifyComponentAdded  // 通知所有 EntityQuery 检查
                    └─► comp->OnAttach()
```

**GetComponent\<T\>()**
```cpp
auto it = components.find(typeid(T).hash_code());
return (it != end) ? static_cast<T*>(it->second.get()) : nullptr;
```

**RemoveComponent\<T\>()**
```
NotifyComponentRemoved()
  └─► UnregisterFromContext()          // 从 component_registry 移除
        └─► comp->OnDetach()
```

### 3.3 EntityID

`EntityID` 是一个生成式句柄，编码 `(slot_index, generation)` 两部分：

- `slot_index`：在 EntityManager 的槽数组中的位置
- `generation`：每次销毁/复用该槽时自增

句柄无效时（generation 不匹配），`EntityManager::GetEntity()` 返回 `nullptr`，彻底避免悬挂引用。

---

## 4. EntityManager —— 实体生命周期管理

文件：`inc/hgl/ecs/core/EntityManager.h`

### 4.1 槽池设计

```cpp
struct EntitySlot
{
    unique_ptr<Entity> entity;
    uint16_t generation = 0;
    bool     alive      = false;
};

vector<EntitySlot>   slots;        // 全量槽数组
vector<uint32_t>     free_indices; // 空闲槽位索引栈
```

### 4.2 关键操作

| 操作 | 行为 |
|------|------|
| `CreateEntity()` | 优先复用 free_indices 栈顶槽位，生成新 EntityID |
| `DestroyEntity(id)` | 验证 generation → 析构 Entity → 递增 generation → 入栈 free_indices |
| `GetEntity(id)` | 验证 generation 是否匹配，匹配才返回指针 |
| `GetEntityCount()` | 返回 alive 实体数 |
| `Clear()` | 清空全部槽，重置 free_indices |

---

## 5. EntityQuery —— 按组件签名检索实体

文件：`inc/hgl/ecs/core/EntityQuery.h`

### 5.1 工作原理

`EntityQuery` 缓存了满足特定组件签名的 `EntityID` 列表，支持三种参与模式：

| 模式 | 说明 |
|------|------|
| **Mandatory** | 拥有所有 `required_components` 即加入 |
| **Conditional** | 满足组件要求 **且** 通过 `Predicate` 过滤 |
| **Manual** | 系统手动调用 Add/Remove 管理缓存 |

### 5.2 缓存维护

```cpp
EntityQuery& WithComponent<T>()    // 添加必要组件条件
EntityQuery& WithPredicate(pred)   // 设置过滤谓词

size_t Rebuild()                   // 全量重扫（新系统安装时）
bool   TryAddEntity(id, entity)    // 增量：Entity 添加组件时
bool   TryRemoveEntity(id)         // 增量：Entity 移除组件时
size_t RemoveInvalidEntities()     // 清理失效句柄
```

`dirty` 标志控制惰性重建；大多数情况下通过增量更新维护缓存，避免每帧全量扫描。

### 5.3 系统使用方式

```cpp
// System 内部
void OnCreate() override
{
    query = CreateQuery<TransformComponent, MeshComponent>();
}

void Update(float dt) override
{
    for (EntityID id : query->GetEntities())
    {
        auto* transform = ctx->GetEntity(id)->GetComponent<TransformComponent>();
        // ...
    }
}
```

---

## 6. System —— 逻辑与渲染处理单元

文件：`inc/hgl/ecs/core/System.h`

### 6.1 执行阶段（ExecutionPhase，共 18 个）

```
TickInput            → 输入读取
TickPrePhysics       → 物理前置
TickPhysics          → 物理模拟
TickPostPhysics      → 物理后置
TickAnimation        → 动画更新
TickTransform        → Transform 树计算
TickAI               → AI 决策
TickGameplay         → 游戏逻辑
TickLate             → 延迟逻辑

RenderPreBeginFrame  → 帧前准备
RenderResourceSetup  → 资源上传
RenderMaterialBind   → 材质绑定
RenderCollect        → 渲染元素收集
RenderCull           → 视锥裁剪
RenderBatch          → 合批
RenderStat           → 统计
RenderDraw           → 绘制命令生成
RenderSubmit         → 命令提交
```

阶段值决定 ECSContext 内系统执行顺序（相同阶段内按 `insertion_order` 稳定排序）。

### 6.2 关键成员

| 成员 | 说明 |
|------|------|
| `executionPhase` | 决定排序位置 |
| `enabled` | 关闭后跳过 Update/Render |
| `context` | 非拥有，指向宿主 ECSContext |
| `render_element_type` | 字符串标识，用于 `systems_by_element_type` 查找 |
| `dependencies` | 依赖系统 key 列表，用于拓扑排序 |

### 6.3 虚函数钩子

```cpp
virtual void Update(float dt)                               {}  // Tick 阶段
virtual void Render(RenderCmdBuffer* cmd, float dt)         {}  // Render 阶段
```

### 6.4 执行顺序控制

```cpp
SetExecutionOrder(ExecutionPhase::TickTransform);
AddDependency<OtherSystem>();   // 必须在 OtherSystem 之后执行
```

ECSContext 在 `SortSystemList()` 中执行拓扑排序（`tick_dependencies` / `render_dependencies`），排序结果缓存于 `tick_system_order` / `render_system_order`，通过 `tick_order_dirty` 标志惰性重建。

---

## 7. ECSContext —— ECS 的核心运行时

文件：`inc/hgl/ecs/core/Context.h`（类名 `ECSContext`，继承自 `Object`）

### 7.1 主要字段

#### 系统管理

```cpp
UnorderedMap<size_t, shared_ptr<System>> tick_systems;    // Tick 系统注册表
UnorderedMap<size_t, shared_ptr<System>> render_systems;  // Render 系统注册表
// key = typeid(T).hash_code()

map<string, vector<shared_ptr<System>>> systems_by_element_type; // 按 render_element_type 索引

vector<OrderedSystem> tick_system_order;    // 排好序的 Tick 执行列表
vector<OrderedSystem> render_system_order;  // 排好序的 Render 执行列表
bool tick_order_dirty   = false;
bool render_order_dirty = false;
uint64_t next_system_order = 1;             // 插入序，用于稳定排序
```

#### 组件注册表

```cpp
UnorderedMap<size_t, vector<weak_ptr<Component>>> component_registry;
// key = typeid(T).hash_code()，弱引用池
```

#### Transform 分离列表

```cpp
vector<weak_ptr<TransformComponent>> static_transforms;   // 静态物体（不每帧更新）
vector<weak_ptr<TransformComponent>> movable_transforms;  // 动态物体
```

#### 渲染资源

```cpp
unordered_map<string, unique_ptr<RenderPipelineBase>> render_pipelines;
RenderFrameCache render_frame_cache;    // 本帧渲染数据：RenderItem, MaterialBatch, CameraInfo
SystemProfiler   profiler;
VulkanDevice*    gpu_device     = nullptr;
IRenderTarget*   render_target  = nullptr;
RenderCmdBuffer* current_render_cmd = nullptr;
unique_ptr<RenderSystemCore> render_core;
```

#### 角色与权限控制

```cpp
enum class ContextRole : uint8_t
{
    RootShared  = 0,  // 根场景，可注册所有系统
    LocalSubWorld = 1 // 子世界，拒绝 GlobalShared 渲染系统注册
};

enum class SystemOwnershipScope : uint8_t
{
    Auto         = 0,
    GlobalShared = 1, // 根渲染系统；LocalSubWorld 拒绝注册
    LocalIsolated = 2 // 子世界独立系统
};
```

#### SystemGroup 追踪

```cpp
unordered_map<string, uint32_t> system_group_component_counts; // 组件引用计数
set<string> installed_system_groups;  // 已安装的组 key
```

### 7.2 生命周期

```
InitializeGraphics(device, target)
  └─► 绑定 gpu_device / render_target
  └─► Initialize()
        └─► 注册默认系统

Tick(deltaTime)
  └─► SortTickSystems() (若 dirty)
  └─► 按 tick_system_order 依次调用 system->Update(dt)

Render(cmd, deltaTime)
  └─► SortRenderSystems() (若 dirty)
  └─► 按 render_system_order 调用 system->Render(cmd, dt)

Shutdown()
  └─► 析构所有系统和 Entity
```

### 7.3 EnsureSystemGroupSystems

当 `Component::GetSystemGroupName()` 返回非空字符串时，ECSContext 检查 `installed_system_groups` 集合：若未安装，则通过 `SystemGroupRegistry` 调用注册函数一次性安装整组系统。这是"数据驱动系统激活"的关键机制。

---

## 8. World —— 场景级编排器

文件：`inc/hgl/ecs/core/World.h`

### 8.1 核心结构

```cpp
class World
{
    shared_ptr<ECSContext>      context;     // 拥有
    vector<shared_ptr<World>>  children;    // 子场景列表（形成树）
    unique_ptr<WorldScheduler> scheduler;   // 调度器（仅根 World 持有有效调度器）

    bool active       = true;
    bool is_ticking   = false;  // 防止 Tick 内再入
    bool is_rendering = false;  // 防止 Render 内再入
};
```

**World 不是 ECSContext 的子类**——World 是场景的外层组织单元，持有并代理 ECSContext。

### 8.2 主要接口

| 接口 | 说明 |
|------|------|
| `GetContext()` | 返回 `ECSContext*` |
| `GetChildren()` | 返回子 World 列表（const 引用） |
| `AddChild(child)` | 添加子场景，触发调度器 topology_dirty |
| `RemoveChild(child)` | 同上 |
| `Tick(dt)` | → scheduler->Rebuild / scheduler->Tick |
| `Render(cmd, dt)` | → scheduler->Rebuild / scheduler->Render |
| `CreateEntity<T>(name)` | 委托给 context 创建 Entity |
| `RegisterTickSystem<T>()` | 委托给 context |
| `RegisterRenderSystem<T>()` | 委托给 context |

### 8.3 调用代理关系

```
World::Tick(dt)
  └─► scheduler->Rebuild(this)      // 若 topology_dirty
  └─► scheduler->Tick(dt)

World::Render(cmd, dt)
  └─► scheduler->Rebuild(this)
  └─► scheduler->Render(cmd, dt)
```

---

## 9. WorldScheduler —— 世界树扁平化调度

文件：`inc/hgl/ecs/core/WorldScheduler.h`  
实现：`src/ecs/core/WorldScheduler.cpp`

### 9.1 核心数据结构

```cpp
struct FlatWorldRecord
{
    World*      world          = nullptr;
    ECSContext* context        = nullptr;
    ECSContext* parent_context = nullptr;

    // SubWorldComponent 三分类
    vector<SubWorldComponent*> logic_subworlds;          // IsLogicIsolated
    vector<SubWorldComponent*> bridge_subworlds;         // IsLogicIsolated && IsRenderShared
    vector<SubWorldComponent*> isolated_render_subworlds; // !IsRenderShared

    size_t   subworld_component_count    = 0;
    uint64_t subworld_policy_signature   = 0;
    bool     lists_valid                 = false;
};

vector<FlatWorldRecord>     flat_worlds;          // DFS 前序遍历 World 树的结果
vector<SubWorldComponent*>  flat_logic_subworlds; // 所有 logic_subworlds 的扁平汇总

bool    topology_dirty = true;
World*  cached_root    = nullptr;

struct SchedulerStats
{
    size_t flat_world_count           = 0;
    size_t logic_subworld_count       = 0;
    size_t bridge_subworld_count      = 0;
    size_t isolated_render_subworld_count = 0;
};
```

### 9.2 Rebuild 流程

```
Rebuild(root)
├─ topology_dirty? ──Yes──► FlattenWorldTree(root, nullptr)
│                            // DFS 遍历，每个 World 生成一条 FlatWorldRecord
│                            // 父 context 传递给子节点的 parent_context
├─ 对每条 FlatWorldRecord 调用 RefreshSubWorldListsIfNeeded()
│    └─ 获取该 context 中所有 SubWorldComponent
│    └─ 按 logic_isolated / render_shared 分类到三个子列表
│    └─ 合并到 flat_logic_subworlds
└─ 更新 SchedulerStats
```

增量刷新：当 `subworld_component_count` 和 `subworld_policy_signature` 均未变化时，跳过该 Record 重分类，避免每帧全量重建。

### 9.3 Tick 流程

```
Tick(delta_time)
└─ for each FlatWorldRecord:
     ├─ SyncChildFrameIndex(parent_ctx, ctx)  // 同步帧号
     ├─ ctx->Tick(delta_time)                 // 执行该 World 自身的 Tick 系统
     └─ for each logic_subworld:
          └─ sub_world->UpdateSubWorld(dt)    // 独立逻辑子世界 Tick
```

### 9.4 Render 流程

```
Render(cmd, delta_time)
└─ for each FlatWorldRecord:
     ├─ SyncChildFrameIndex(parent_ctx, ctx)
     ├─ for each bridge_subworld:
     │    └─ sub_world->SyncSharedRenderBridge(dt)  // 桥接子世界同步渲染数据
     ├─ ctx->Render(cmd, dt)                         // 该 World 渲染系统执行
     └─ for each isolated_render_subworld:
          └─ sub_world->RenderSubWorld(cmd, dt)      // 独立渲染子世界单独绘制
```

---

## 10. SubWorldComponent —— 嵌套世界的桥梁

文件：`inc/hgl/ecs/components/SubWorldComponent.h`

### 10.1 概念

`SubWorldComponent` 是挂载在 Entity 上的特殊组件，它内嵌一个完整的 `World`，实现了 ECS 世界的树形嵌套：

```
RootWorld
  └── Entity "House"
        └── SubWorldComponent
              └── SubWorld (World)
                    └── Entity "Door"
                    └── Entity "Window"
```

### 10.2 关键字段

| 字段 | 类型 | 说明 |
|------|------|------|
| `sub_world` | `shared_ptr<World>` | 内嵌子世界拥有权 |
| `mode` | `SubWorldMode` | SharedContext / IsolatedContext |
| `render_shared` | `bool` | 与父上下文共享渲染（默认 true） |
| `logic_isolated` | `bool` | 逻辑独立执行（默认 false） |
| `paused` | `bool` | 暂停子世界更新 |
| `tick_enabled` | `bool` | 是否参与 Tick |
| `render_enabled` | `bool` | 是否参与 Render |

### 10.3 三种子世界分类

WorldScheduler 的 `RefreshSubWorldListsIfNeeded()` 根据标志将 SubWorldComponent 分入三个列表：

| 分类 | 条件 | 调度行为 |
|------|------|---------|
| `logic_subworlds` | `IsLogicIsolated() == true` | 独立调用 `UpdateSubWorld(dt)` |
| `bridge_subworlds` | `IsLogicIsolated() && IsRenderShared()` | 调用 `SyncSharedRenderBridge(dt)`，桥接共享 Transform |
| `isolated_render_subworlds` | `!IsRenderShared()` | 独立调用 `RenderSubWorld(cmd, dt)` |

默认（`render_shared=true, logic_isolated=false`）的子世界不进入任何分类，由父 World 的 Scheduler 统一处理其 Context 的 Tick/Render。

### 10.4 与 SubWorldMode 的关系

```cpp
enum class SubWorldMode : uint8_t
{
    SharedContext   = 0,  // 子世界共享父 ECSContext（轻量）
    IsolatedContext = 1   // 子世界拥有独立 ECSContext（完整隔离）
};
```

`Initialize(parent_context)` 根据 mode 决定是共享还是新建 ECSContext；两种模式都通过共享父级 `TransformDataStorage` 实现跨 Context 的父子 Transform 关系。

---

## 11. 所有权与生命周期图

```
Application
  └── World (root)                  [unique_ptr / shared_ptr]
        ├── ECSContext               [shared_ptr, World 拥有]
        │     ├── EntityManager      [unique_ptr]
        │     │     └── EntitySlot[] → Entity  [unique_ptr]
        │     │                            └── Component [shared_ptr]
        │     ├── tick_systems[]     [shared_ptr<System>]
        │     ├── render_systems[]   [shared_ptr<System>]
        │     ├── component_registry [weak_ptr<Component>]
        │     └── render_pipelines[] [unique_ptr<RenderPipelineBase>]
        │
        ├── WorldScheduler           [unique_ptr, by root World]
        │     └── flat_worlds[]  (FlatWorldRecord, non-owning ptrs)
        │
        └── children[]              [shared_ptr<World>]
              └── (子 World 拥有自己的 ECSContext + 子树)
                    └── Entity with SubWorldComponent
                          └── sub_world [shared_ptr<World>]
                                └── 递归嵌套...
```

---

## 12. 一帧完整执行流程

```
App::MainLoop()
│
├─ World::Tick(dt)
│    ├─ WorldScheduler::Rebuild(root)        // 若 topology_dirty
│    └─ WorldScheduler::Tick(dt)
│         └─ for each FlatWorldRecord:
│              ├─ SyncChildFrameIndex()
│              ├─ ECSContext::Tick(dt)
│              │    ├─ SortTickSystems()     // 若 order_dirty
│              │    └─ for each OrderedSystem (TickInput → TickLate):
│              │         └─ system->Update(dt)
│              └─ for each logic_subworld:
│                   └─ SubWorldComponent::UpdateSubWorld(dt)
│
└─ World::Render(cmd, dt)
     ├─ WorldScheduler::Rebuild(root)        // 同上
     └─ WorldScheduler::Render(cmd, dt)
          └─ for each FlatWorldRecord:
               ├─ SyncChildFrameIndex()
               ├─ for each bridge_subworld → SyncSharedRenderBridge(dt)
               ├─ ECSContext::Render(cmd, dt)
               │    ├─ SortRenderSystems()   // 若 order_dirty
               │    └─ for each OrderedSystem (RenderPreBeginFrame → RenderSubmit):
               │         └─ system->Render(cmd, dt)
               └─ for each isolated_render_subworld:
                    └─ SubWorldComponent::RenderSubWorld(cmd, dt)
```

---

## 13. 关键设计要点总结

| 设计点 | 机制 |
|--------|------|
| **数据驱动系统激活** | `Component::GetSystemGroupName()` 触发 `EnsureSystemGroupSystems`，无需手动安装 |
| **稳定排序** | `insertion_order` 字段保证同 phase 内系统顺序可预测 |
| **拓扑依赖** | `AddDependency<T>()` + `SortSystemList()` 的拓扑排序，确保依赖先执行 |
| **脏标志惰性重建** | `tick_order_dirty`、`topology_dirty`、`EntityQuery::dirty` 均为惰性求值 |
| **生成式句柄** | `EntityID(index+generation)` 防止悬挂引用，O(1) 验证 |
| **Transform 分离** | `static_transforms` vs `movable_transforms` 减少不必要的矩阵计算 |
| **Context 角色门控** | `ContextRole::LocalSubWorld` + `SystemOwnershipScope::GlobalShared` 阻止渲染系统重复安装 |
| **子世界三分类** | `logic_isolated` / `render_shared` 组合决定调度路径，支持灵活的多世界拓扑 |
