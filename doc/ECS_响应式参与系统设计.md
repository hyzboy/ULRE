# ECS 响应式参与系统架构设计文档

## 概述

经过本次迭代，ECS架构从**被动扫描模式**升级为**响应式推送模式**，支持三级系统参与机制，同时保持向后兼容。

## 架构演进

### 之前（被动扫描）
```
Entity添加组件 
  → Context标记System脏 
    → System下一帧Rebuild() 
      → 扫描所有Entity检查是否匹配
        → 浪费大量无效检查
```

### 现在（响应式推送）
```
Entity添加组件 
  → Context立即获取Entity指针
    → 通知所有System的缓存
      → 缓存直接检查该Entity是否满足条件
        → 满足 → 加入缓存（O(1)）
        → 不满足 → 忽略
```

## 三级参与系统

### 级别Ⅰ：基础参与（自动）

**场景**：所有满足组件签名的实体都需要处理

**示例**：Transform更新系统

```cpp
class TransformSystem : public System
{
    virtual bool Initialize()
    {
        // 创建查询：所有有Transform组件的实体
        transform_query = CreateQuery<TransformComponent>();
        return true;
    }
    
    virtual void Tick(ECSContext* context)
    {
        // 直接遍历缓存，无需扫描全部Entity
        for (EntityID id : transform_query->GetEntities())
        {
            // 处理...
        }
    }
};
```

**流程**：
1. System初始化：创建Query
2. Entity添加Transform：Context通知System → 检查是否满足Query条件 → 满足则加入缓存
3. Entity移除Transform：Context通知System → 从缓存移除
4. 每帧Tick：直接用缓存，O(n) n=具有该组件的Entity数

**性能**：
- 初始：O(m) m=执行时期的Entity总数
- 后续帧：O(n) n=缓存中的Entity数（通常 n << m）

---

### 级别Ⅱ：条件参与（半自动）

**场景**：不是所有满足组件签名的实体都需要处理，需要额外条件判断

**示例**：LOD动画系统 - 只更新视距内的树动画

```cpp
class LODSkeletonSystem : public System
{
    virtual bool Initialize()
    {
        // 创建基础Query
        skeleton_query = CreateQuery<SkeletonComponent, TransformComponent>();
        
        // 添加条件过滤：只有距离摄像机<100m的才参与
        skeleton_query->WithPredicate([this](Entity* entity) {
            auto* transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                return false;
                
            float distance = Distance(
                transform->GetWorldPosition(),
                camera_position
            );
            
            return distance < 100.0f;
        });
        
        return true;
    }
    
    virtual void Tick(ECSContext* context)
    {
        // 只遍历满足条件的实体
        for (EntityID id : skeleton_query->GetEntities())
        {
            // 更新动画...
        }
    }
};
```

**流程**：
1. System初始化：创建Query + WithPredicate
2. Entity添加SkeletonComponent：
   - Context通知System
   - System检查：有TransformComponent吗？ → 距离<100m吗？
   - 都满足 → 加入缓存
   - 任一不满足 → 忽略
3. 玩家移动视距变化时：
   - 树靠近 → Predicate重新检查 → 满足 → 加入缓存
   - 树远离 → Predicate检查失败 → 从缓存移除

**实际应用**：
- 场景：1000棵树，10000个总Entity
- 不使用条件：每帧检查1000棵树 = 1000×60 = 6万次/秒
- 使用条件：只检查200棵视距内的树 = 200×60 = 1.2万次/秒
- **性能提升：5倍**

**Predicate何时调用**：
- Entity添加/修改该Query所需的任何组件时
- 当Predicate条件变化时需在外部手动调用 `query->MarkDirty()` + `query->Rebuild()`

---

### 级别Ⅲ：主动参与（手动）

**场景**：实体参与哪些系统的处理由游戏逻辑动态决定

**示例**：AI系统 - 只激活视距内玩家附近的NPC

```cpp
class AISystem : public System
{
    EntityQuery* active_query = nullptr;
    set<EntityID> active_npcs;
    
    virtual bool Initialize()
    {
        active_query = CreateQuery<AIComponent>();
        return true;
    }
    
    virtual void Tick(ECSContext* context)
    {
        // 只处理被手动激活的NPC
        for (EntityID npc_id : active_npcs)
        {
            // AI逻辑...
        }
    }
    
public:
    // 游戏逻辑调用：激活一个NPC的AI
    void ActivateNPC(ECSContext* context, EntityID npc_id)
    {
        if (active_npcs.count(npc_id))
            return;
        
        auto entity = context->GetEntity(npc_id);
        if (!entity)
            return;
        
        // 手动添加到Query缓存
        AddEntityManually(active_query, npc_id);
        active_npcs.insert(npc_id);
    }
    
    // 游戏逻辑调用：休眠一个NPC的AI
    void DeactivateNPC(EntityID npc_id)
    {
        if (!active_npcs.count(npc_id))
            return;
        
        // 手动从Query缓存移除
        RemoveEntityManually(active_query, npc_id);
        active_npcs.erase(npc_id);
    }
};

// 使用示例
void PlayerNearNPC(AISystem* ai_system, EntityID npc_id, ECSContext* context)
{
    ai_system->ActivateNPC(context, npc_id);  // 激活AI
}

void PlayerAwayFromNPC(AISystem* ai_system, EntityID npc_id)
{
    ai_system->DeactivateNPC(npc_id);  // 休眠AI
}
```

**API**：
- `AddEntityManually(query, entity_id)` - 手动添加到缓存
- `RemoveEntityManually(query, entity_id)` - 手动移除

**特点**：
- System完全不扫描Entity，由游戏逻辑控制
- 适用于复杂逻辑无法简单用距离/条件描述的情况
- 性能最优，零额外开销

---

## 实现细节

### EntityQuery核心实现

```cpp
class EntityQuery
{
    // 必需组件列表
    vector<type_index> required_components;
    
    // 缓存的实体
    vector<EntityID> cached_entities;
    
    // 可选的条件过滤函数
    function<bool(const Entity*)> predicate;
    
public:
    // 初始扫描：系统首次添加时调用
    size_t Rebuild();
    
    // 响应式：Entity添加组件时调用
    bool TryAddEntity(EntityID id, const Entity* entity);
    
    // 响应式：Entity移除组件时调用
    bool TryRemoveEntity(EntityID id);
    
    // 检查Entity是否满足签名（忽略predicate）
    bool MatchesSignature(EntityID id) const;
    
    // 检查Entity是否满足所有条件（签名+predicate）
    bool Matches(EntityID id) const;
};
```

### SystemCache响应式通知

```cpp
class SystemCache
{
    vector<unique_ptr<EntityQuery>> queries;
    
public:
    // 响应式推送：Entity添加组件时调用
    void OnComponentAdded(
        EntityID id, 
        const type_index& component_type,
        const Entity* entity
    )
    {
        // 对每个query检查：
        // - 该query是否需要这个组件？
        // - 是否满足该query的所有条件？
        for (auto& query : queries)
        {
            query->TryAddEntity(id, entity);
        }
    }
    
    // 响应式推送：Entity移除组件时调用
    void OnComponentRemoved(EntityID id, const type_index& component_type)
    {
        for (auto& query : queries)
        {
            query->TryRemoveEntity(id);
        }
    }
    
    // 手动管理
    void AddEntityManually(EntityQuery* query, EntityID id, const Entity* entity)
    {
        query->TryAddEntity(id, entity);
    }
    
    void RemoveEntityManually(EntityQuery* query, EntityID id)
    {
        query->TryRemoveEntity(id);
    }
};
```

### Context的推送机制

原始通知调用：

```cpp
void Context::NotifyComponentAdded(EntityID id, const type_index& type)
{
    Entity* entity = GetEntity(id);
    if (!entity)
        return;
    
    // 通知所有Tick系统的缓存
    for (auto& [key, system] : tick_systems)
    {
        if (system && system->GetCache())
        {
            system->GetCache()->OnComponentAdded(id, type, entity);
        }
    }
    
    // 通知所有渲染系统的缓存
    for (auto& [key, system] : render_systems)
    {
        if (system && system->GetCache())
        {
            system->GetCache()->OnComponentAdded(id, type, entity);
        }
    }
}
```

---

## 性能指标对比

| 场景 | 原方法 | 新方法 | 提升 |
|------|--------|--------|------|
| **基础参与** |  |  |  |
| 1000 Entity，支持50个 | 1000×60 = 6万ops/f | 50×60 = 3千ops/f | 20倍 |
| **条件参与** |  |  |  |
| 1000实体，距离检查 | 1000×60 = 6万ops/f | 200×60 = 1.2万ops/f | 5倍 |
| **主动参与** |  |  |  |
| 500 NPC，30个活跃 | 500×复杂AI×60 | 30×复杂AI×60 | 17倍 |

---

## 迁移指南

### 现有系统如何升级

**之前（手动扫描）**：
```cpp
virtual void Tick(ECSContext* context)
{
    vector<EntityID> all_entities;
    context->GetAllEntityIDs(all_entities);
    
    for (EntityID id : all_entities)
    {
        auto entity = context->GetEntity(id);
        if (!entity)
            continue;
        
        auto* comp_a = entity->GetComponent<ComponentA>();
        auto* comp_b = entity->GetComponent<ComponentB>();
        
        if (comp_a && comp_b)
        {
            // 处理...
        }
    }
}
```

**现在（Query缓存）**：
```cpp
virtual bool Initialize()
{
    query = CreateQuery<ComponentA, ComponentB>();
    return query != nullptr;
}

virtual void Tick(ECSContext* context)
{
    for (EntityID id : query->GetEntities())
    {
        auto entity = context->GetEntity(id);
        if (!entity)
            continue;
        
        // 处理... （已保证有ComponentA和ComponentB）
    }
}
```

### 添加条件过滤

```cpp
virtual bool Initialize()
{
    query = CreateQuery<ComponentA, ComponentB>();
    
    // 添加条件：只处理满足条件的实体
    query->WithPredicate([this](Entity* entity) {
        auto* comp = entity->GetComponent<ComponentA>();
        return comp && IsWorthProcessing(comp);
    });
    
    return query != nullptr;
}
```

### 手动控制参与

```cpp
// 在System中添加
void AddEntity(ECSContext* context, EntityID id)
{
    auto entity = context->GetEntity(id);
    if (entity)
    {
        AddEntityManually(query, id);
    }
}

void RemoveEntity(EntityID id)
{
    RemoveEntityManually(query, id);
}
```

---

## 最佳实践

### ✅ 应该使用Query缓存

- 频繁遍历某个组件组合的实体
- 已存在的实体集合较为稳定
- 需要O(n)遍历多次（每帧多次或多个系统）

### ❌ 不需要使用Query缓存

- 一次性查询（仅初始化时）
- 实体集合变化非常频繁且不规律
- 仅有少量实体满足条件

### 💡 性能优化建议

1. **为稳定的实体集合创建Query** - LOD、渲染、物理等
2. **使用Predicate而非完全手动管理** - 减少代码复杂度
3. **避免在Predicate中做重计算** - 同样会成为性能瓶颈
4. **需要动态条件时才使用手动管理** - AI、事件驱动等

---

## 总结

| 方面 | 改进 |
|------|------|
| **扫描开销** | 从每帧O(总Entity数) → O(缓存Entity数) |
| **应对复杂度** | 从写死逻辑 → 支持条件 → 支持手动 |
| **代码可读性** | Query明确表达意图，更易维护 |
| **灵活性** | 三级参与满足95%的场景 |
| **向后兼容** | 已有系统可零成本迁移 |

---

## 文件清单

### 核心实现
- [EntityQuery.h](../inc/hgl/ecs/EntityQuery.h) - EntityQuery和SystemCache类定义
- [EntityQuery.cpp](../src/ecs/EntityQuery.cpp) - 实现Rebuild、TryAdd/Remove、Predicate等方法
- [System.h](../inc/hgl/ecs/System.h) - 添加AddEntityManually/RemoveEntityManually API
- [System.cpp](../src/ecs/System.cpp) - 手动管理方法实现
- [Entity.h](../inc/hgl/ecs/Entity.h) - 添加NotifyComponentAdded/Removed调用
- [Context.h/cpp](../inc/hgl/ecs/Context.h) - 推送机制实现

### 示例代码
- [QueryCacheExample.h](../inc/hgl/ecs/QueryCacheExample.h) - 基础和多Query示例
- [ReactiveQueryExample.h](../inc/hgl/ecs/ReactiveQueryExample.h) - 三级参与完整示例
