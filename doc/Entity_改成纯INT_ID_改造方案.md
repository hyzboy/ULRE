# Entity 改成纯 INT ID 改造方案

**日期**: 2026年2月9日  
**优先级**: 中等  
**复杂度**: 高  
**工作量**: 5-7天

---

## 概述

将 Entity 从 `std::shared_ptr<Entity>` 改成纯 INT ID（EntityID）系统。这是一个重大的架构升级，能带来以下好处：

✅ **内存更高效** - 不需要 shared_ptr 的开销  
✅ **缓存友好** - ID 是轻量级的 int，便于批处理  
✅ **序列化简单** - 只需保存 ID，不需要处理指针  
✅ **网络传输简单** - ID 易于网络同步  
✅ **防止悬空指针** - 通过版本号检测失效引用  

但也会带来编码复杂度增加和性能可能需要调优。

---

## 当前状态分析

### 现有 Entity 引用位置（14 处）

**Header 文件**:
- `Context.h` (1处): `std::vector<std::shared_ptr<Entity>> entities;`
- `Component.h` (2处): owner 相关（弱引用）
- `Entity.h` (源定义)
- `PrimitiveRenderItem.h` (3处): entity 成员
- `TransformComponent.h` (5处): parent/children 相关
- `RenderItem.h` (1处): 虚函数

**Implementation 文件**:
- `TransformComponent.cpp` (3处): SetParent, AddChild, RemoveChild
- `PrimitiveRenderItem.cpp` (1处): 构造函数

---

## 改造步骤

### 第一阶段：基础 ID 系统（1-2天）

#### 1.1 创建 EntityID 和 EntityReference

创建新文件 `inc/hgl/ecs/EntityHandle.h`：

```cpp
#pragma once

#include <cstdint>
#include <limits>

namespace hgl::ecs
{
    /// EntityID - lightweight handle to an entity
    struct EntityID
    {
        uint32_t index = UINT32_MAX;      ///< Index in the entity pool
        uint16_t generation = 0;          ///< Generation for detecting stale handles
        uint16_t reserved = 0;            ///< Reserved for future use
        
        EntityID() = default;
        EntityID(uint32_t idx, uint16_t gen) : index(idx), generation(gen) {}
        
        /// Check if this ID is valid
        bool IsValid() const { return index != UINT32_MAX; }
        
        /// Create invalid ID
        static EntityID Invalid() { return EntityID(UINT32_MAX, 0); }
        
        bool operator==(const EntityID& other) const 
        {
            return index == other.index && generation == other.generation;
        }
        
        bool operator!=(const EntityID& other) const 
        {
            return !(*this == other);
        }
    };
    
    /// EntityReference - safe wrapper around EntityID
    class EntityReference
    {
    private:
        EntityID id;
        class ECSContext* context = nullptr;
        
    public:
        EntityReference() = default;
        EntityReference(EntityID id, ECSContext* ctx = nullptr) 
            : id(id), context(ctx) {}
        
        EntityID GetID() const { return id; }
        bool IsValid() const;
        
        // Conversion operator for compatibility
        operator EntityID() const { return id; }
    };
}

namespace std
{
    template<>
    struct hash<hgl::ecs::EntityID>
    {
        size_t operator()(const hgl::ecs::EntityID& id) const
        {
            return ((size_t)id.index << 16) | (size_t)id.generation;
        }
    };
}
```

#### 1.2 创建 EntityPool 和 EntityManager

创建新文件 `inc/hgl/ecs/EntityManager.h`：

```cpp
#pragma once

#include<hgl/ecs/EntityHandle.h>
#include<vector>
#include<memory>

namespace hgl::ecs
{
    class Entity;
    
    /// Manages entity lifecycle using ID system
    class EntityManager
    {
    private:
        struct EntitySlot
        {
            std::unique_ptr<Entity> entity;
            uint16_t generation = 0;
            bool alive = false;
        };
        
        std::vector<EntitySlot> slots;
        std::vector<uint32_t> free_indices;
        uint32_t max_entities = 10000;
        
    public:
        EntityManager(uint32_t capacity = 1000);
        ~EntityManager();
        
        /// Create new entity
        EntityID CreateEntity(const std::string& name = "Entity");
        
        /// Destroy entity
        void DestroyEntity(EntityID id);
        
        /// Get entity by ID
        Entity* GetEntity(EntityID id);
        const Entity* GetEntity(EntityID id) const;
        
        /// Check if ID is valid
        bool IsValidID(EntityID id) const;
        
        /// Get all alive entities
        void GetAllEntities(std::vector<EntityID>& out_ids);
        
        /// Clear all entities
        void Clear();
        
    private:
        void ExpandSlots(uint32_t new_capacity);
    };
}
```

---

### 第二阶段：修改 Entity 类（2天）

#### 2.1 修改 Entity.h

```cpp
// 旧方式
#include<memory>
class Entity : public Object, public std::enable_shared_from_this<Entity> { ... }

// 新方式
#include<hgl/ecs/EntityHandle.h>
class Entity : public Object
{
private:
    EntityID id;
    ECSContext* context = nullptr;
    std::unordered_map<std::size_t, std::shared_ptr<Component>> components;
    
public:
    Entity(const std::string& name = "Entity");
    
    EntityID GetID() const { return id; }
    
    // 移除 enable_shared_from_this 相关的 shared_ptr 用法
    // shared_ptr<T> AddComponent 改为返回 T*
    
    // 原来: std::shared_ptr<T> AddComponent(Args...)
    // 新版: T* AddComponent(Args...)
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        auto component = std::make_shared<T>(std::forward<Args>(args)...);
        components[typeid(T).hash_code()] = component;
        component->SetOwner(id);  // 传递 ID 而不是 shared_ptr
        RegisterToContext(typeid(T).hash_code(), component);
        component->OnAttach();
        return component.get();
    }
    
    // 原来: std::shared_ptr<T> GetComponent()
    // 新版: T* GetComponent()
    template<typename T>
    T* GetComponent() const
    {
        auto it = components.find(typeid(T).hash_code());
        if (it != components.end())
        {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }
};
```

#### 2.2 修改 Component.h

```cpp
// 原来
std::weak_ptr<Entity> owner;
void SetOwner(std::shared_ptr<Entity> entity) { owner = entity; }
std::shared_ptr<Entity> GetOwner() const { return owner.lock(); }

// 新版
EntityID owner_id;
ECSContext* owner_context = nullptr;

void SetOwner(EntityID id, ECSContext* ctx = nullptr) 
{ 
    owner_id = id;
    owner_context = ctx;
}

Entity* GetOwner() const;  // 实现中通过 ID 查找
EntityID GetOwnerID() const { return owner_id; }
```

#### 2.3 修改 TransformComponent.h

```cpp
// 原来
std::weak_ptr<Entity> parentEntity;
std::vector<std::shared_ptr<Entity>> childEntities;

void SetParent(std::shared_ptr<Entity> parent);
void AddChild(std::shared_ptr<Entity> child);
void RemoveChild(std::shared_ptr<Entity> child);
const std::vector<std::shared_ptr<Entity>>& GetChildren() const;

// 新版
EntityID parent_id;
std::vector<EntityID> child_ids;

void SetParent(EntityID parent_id);
Entity* GetParent() const;

void AddChild(EntityID child_id);
void RemoveChild(EntityID child_id);
const std::vector<EntityID>& GetChildren() const { return child_ids; }

// 获取 Child Entity 的便利函数
void GetChildEntities(std::vector<Entity*>& out) const;
```

---

### 第三阶段：修改 Context（2-3天）

#### 3.1 修改 Context.h

```cpp
// 原来
std::vector<std::shared_ptr<Entity>> entities;

template<typename T = Entity, typename... Args>
std::shared_ptr<T> CreateEntity(Args&&... args)
{
    auto entity = std::make_shared<T>(std::forward<Args>(args)...);
    entity->SetContext(this);
    entities.push_back(entity);
    entity->OnCreate();
    return entity;
}

const std::vector<std::shared_ptr<Entity>>& GetEntities() const { return entities; }

// 新版
std::unique_ptr<EntityManager> entity_manager;

template<typename T = Entity, typename... Args>
EntityID CreateEntity(Args&&... args)
{
    EntityID id = entity_manager->CreateEntity();
    Entity* entity = entity_manager->GetEntity(id);
    // 初始化 entity...
    entity->OnCreate();
    return id;
}

Entity* GetEntity(EntityID id) 
{
    return entity_manager->GetEntity(id);
}

void GetAllEntities(std::vector<EntityID>& out) 
{
    entity_manager->GetAllEntities(out);
}
```

#### 3.2 修改 GetEntities() 返回值

需要改变所有依赖这个函数的代码：

```cpp
// 原来：遍历 shared_ptr
const auto& entities = context->GetEntities();
for (auto entity : entities)
{
    // use entity
}

// 新版：遍历 ID，然后获取指针
std::vector<EntityID> entity_ids;
context->GetAllEntities(entity_ids);
for (EntityID id : entity_ids)
{
    Entity* entity = context->GetEntity(id);
    if (entity)
    {
        // use entity
    }
}
```

---

### 第四阶段：修改渲染系统（1-2天）

#### 4.1 修改 RenderItem.h 和 PrimitiveRenderItem.h

```cpp
// PrimitiveRenderItem.h
class PrimitiveRenderItem : public RenderItem
{
private:
    EntityID entity_id;  // 改为 ID
    ECSContext* context = nullptr;
    
public:
    PrimitiveRenderItem(
        EntityID ent_id,
        TransformComponent* trans,
        PrimitiveComponent* prim,
        ECSContext* ctx)
        : entity_id(ent_id), context(ctx), transform(trans), primitive(prim)
    {
    }
    
    EntityID GetEntityID() const override { return entity_id; }
    Entity* GetEntity() const override;  // 实现中通过 ID 查找
};

// RenderItem.h (虚基类)
class RenderItem
{
public:
    virtual EntityID GetEntityID() const = 0;
    virtual Entity* GetEntity() const = 0;
};
```

#### 4.2 修改 RenderPrimitiveCollectSystem.cpp

```cpp
// 原来
for (const auto& primitiveComp : primitives)
{
    auto entity = primitiveComp->GetOwner();
    if (!entity) continue;
    
    auto item = std::make_unique<PrimitiveRenderItem>(entity, transform, primitiveComp);
}

// 新版
for (const auto& primitiveComp : primitives)
{
    EntityID entity_id = primitiveComp->GetOwnerID();
    Entity* entity = world->GetEntity(entity_id);
    if (!entity) continue;
    
    auto item = std::make_unique<PrimitiveRenderItem>(
        entity_id, transform, primitiveComp.get(), world);
}
```

---

### 第五阶段：修改 System 和其他代码（1-2天）

#### 5.1 更新所有获取 Entity 的代码

需要搜索并修改所有以下模式：

```cpp
// 模式1：GetOwner()
auto entity = component->GetOwner();
// 改为
Entity* entity = component->GetOwner();  // 或
EntityID entity_id = component->GetOwnerID();

// 模式2：GetEntities()
const auto& entities = context->GetEntities();
for (auto entity : entities) { ... }
// 改为
std::vector<EntityID> entity_ids;
context->GetAllEntities(entity_ids);
for (EntityID id : entity_ids) { ... }

// 模式3：GetParent/GetChildren
auto parent = transform->GetParent();
// 改为
Entity* parent = transform->GetParent();  // 或
EntityID parent_id = transform->GetParentID();
```

#### 5.2 需要修改的文件列表

使用以下命令搜索：
```bash
grep -r "shared_ptr<Entity>" src/ecs/ inc/hgl/ecs/
grep -r "GetOwner()" src/ecs/ inc/hgl/ecs/
grep -r "GetEntities()" src/ecs/ inc/hgl/ecs/
grep -r "GetParent()" src/ecs/ inc/hgl/ecs/
grep -r "GetChildren()" src/ecs/ inc/hgl/ecs/
```

**需要修改的文件**:
- `src/ecs/Component.cpp` - GetOwner 实现
- `src/ecs/TransformComponent.cpp` - SetParent, AddChild, RemoveChild 实现
- `src/ecs/PrimitiveRenderItem.cpp` - 构造和 GetEntity 实现
- `src/ecs/RenderPrimitiveCollectSystem.cpp` - 收集逻辑
- `src/ecs/RenderPrimitiveBatchSystem.cpp` - 查找 owner
- `src/ecs/RenderPrimitiveSubmitSystem.cpp` - 如果使用 owner
- 任何其他访问 Entity 指针的 System

---

## 核心设计决策

### 决策 1：ID 方案选择

```cpp
// 方案 A：简单 ID（推荐，当前方案）
struct EntityID
{
    uint32_t index;      // 池中的索引
    uint16_t generation; // 版本号
};
// 优点：简单，占用内存少（8字节）
// 缺点：需要维护池，需要版本检查

// 方案 B：全局递增 ID
using EntityID = uint64_t;  // 简单递增计数
// 优点：更简单，无版本问题
// 缺点：ID 会耗尽，慢查询需要 map

// 方案 C：哈希 ID
using EntityID = uint64_t;  // 哈希值
// 优点：随机分布，易于网络传输
// 缺点：可能碰撞，序列化困难
```

**推荐使用方案 A**（已在上述代码中体现）。

### 决策 2：组件所有权

```cpp
// 原来：Component 持有 shared_ptr<Entity>（循环引用风险）
class Component
{
    std::weak_ptr<Entity> owner;
};

// 改为：Component 持有 EntityID
class Component
{
    EntityID owner_id;
    ECSContext* owner_context;
    
    Entity* GetOwner() const
    {
        return owner_context ? owner_context->GetEntity(owner_id) : nullptr;
    }
};
// 优点：无循环引用，生命周期清晰
// 缺点：需要 context 指针
```

### 决策 3：返回值类型

```cpp
// 原来
std::shared_ptr<Entity> AddComponent(Args...);
std::shared_ptr<Entity> GetParent() const;

// 改为（选项）
// A. 返回裸指针（简单，但需要 Context）
Entity* GetParent() const { return context->GetEntity(parent_id); }

// B. 返回 ID（类型安全，但需要再次查询）
EntityID GetParentID() const { return parent_id; }

// C. 同时提供两种
Entity* GetParent() const { ... }
EntityID GetParentID() const { return parent_id; }
```

**推荐采用方案 C**（灵活，兼容性最好）。

### 决策 4：错误处理

```cpp
// 使用版本号检测无效 ID
Entity* EntityManager::GetEntity(EntityID id)
{
    if (id.index >= slots.size())
        return nullptr;
    
    EntitySlot& slot = slots[id.index];
    if (!slot.alive || slot.generation != id.generation)
        return nullptr;  // ID 已失效
    
    return slot.entity.get();
}

// 使用断言检测（调试模式）
#ifdef _DEBUG
Entity* GetOwner() const
{
    Entity* entity = owner_context->GetEntity(owner_id);
    assert(entity != nullptr);  // 如果为空，说明引用无效
    return entity;
}
#else
Entity* GetOwner() const
{
    return owner_context ? owner_context->GetEntity(owner_id) : nullptr;
}
#endif
```

---

## 兼容性和迁移

### 提供过渡 API

为了减少重构工作，可以提供过渡 API：

```cpp
// Entity.h
class Entity
{
public:
    // 新 API
    EntityID GetID() const { return id; }
    T* AddComponent(Args...) { ... }
    T* GetComponent() const { ... }
    
    // 过渡 API（稍后删除）
    std::shared_ptr<Entity> GetSharedPtr() 
    { 
        // 模拟共享指针行为（不推荐用于长期存储）
        return context->GetEntitySharedPtr(id);
    }
};
```

### 分阶段迁移

1. **第一阶段**：实现 ID 系统，保持共存
2. **第二阶段**：逐个系统改为使用 ID
3. **第三阶段**：移除过渡 API，完全切换

---

## 性能影响

### 预期改进

- **内存占用**: ↓ 20-30%（减少共享指针开销）
- **缓存友好度**: ↑ 提升（小 ID 更易缓存）
- **序列化速度**: ↑ 提升（ID 直接保存，无需特殊处理）
- **Entity 查询**: ↑ 提升（数组索引，O(1) 查询）

### 可能下降

- **首次查询**: ↓ 轻微下降（需要检查版本号）
- **RenderItem 创建**: ↓ 轻微下降（需要多传参数）

---

## 测试计划

1. **单元测试**（EntityManager）
   - 创建/销毁 ID 有效性
   - 版本号检测
   - 池扩展

2. **集成测试**（ECS 系统）
   - Entity 生命周期
   - Component 关联
   - Transform 层级

3. **性能测试**
   - 大量 Entity 创建/销毁
   - Component 查询性能
   - 渲染收集性能

4. **回归测试**
   - 现有功能完整性
   - 渲染管线正确性

---

## 风险和落地建议

### 风险

1. **大工作量** - 需要改动多个文件
2. **兼容性问题** - 外部代码可能依赖 shared_ptr
3. **调试困难** - ID 比指针更难调试
4. **性能不确定** - 实际性能需要测试

### 时间估计

- **分析和设计**: 1 天
- **核心实现** (EntityManager 等): 1-2 天
- **Entity 和 Component 改造**: 1-2 天
- **Context 和 System 改造**: 1-2 天
- **测试和修复**: 1-2 天
- **总计**: **5-7 天**

### 建议

1. **先做原型**: 在分支上完成改造，不影响主线
2. **增量迁移**: 不要一次全改，一个文件一个文件来
3. **充分测试**: 特别是 Entity 生命周期和 ID 有效性
4. **文档更新**: 更新 ECS 使用文档
5. **代码审查**: 请有经验的团队成员审查

---

## 参考实现

建议参考的高性能 ECS 实现：

- **EnTT** (C++17): 使用 Entity 作为整数 ID
- **Sparse Set 数据结构**: 用于高效 Entity 存储
- **Archetype 系统**: 结合 ID 使用，缓存效果更好

---

## 总结

将 Entity 改成纯 INT ID 是一个重要的架构升级，涉及面广但价值大。关键是：

✅ **设计清晰** - ID + 版本号方案  
✅ **分阶段实施** - 不要一次全改  
✅ **充分测试** - 特别需要关注生命周期  
✅ **保持文档** - 新增 API 需要好文档  

建议先在非关键分支上做完整原型，验证设计可行性后再逐步整合。
