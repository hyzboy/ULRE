# ECS 创建与组件掩码（Mask）整体方案

> 目标：让创建角色/对象不再“每次手写一堆组件”，并让多个System共享组件时保持清晰、性能高、可扩展。

## 1. 设计核心思想

### 1.1 Entity只是ID
- Entity只包含一个ID，用来索引组件数据。
- 组件（Component）是纯数据。
- System只负责逻辑，不持有数据。

### 1.2 统一组件管理器（EntityManager）
- 所有组件数据集中存储（避免多System重复保存）。
- 每个Entity有一个组件掩码（`mask`），用于快速判断是否具备某些组件。
- System通过掩码筛选要处理的Entity。

### 1.3 Factory + Template封装创建
- Factory封装常见对象的“创建套路”，一行代码创建完整对象。
- Template支持数据驱动，可被关卡编辑器、配置文件、MOD读取。

## 2. 组件掩码（Mask）价值

掩码是ECS筛选的核心：

- **O(1)判断Entity是否有某组件**
- **位运算快速过滤**：
  ```cpp
  if ((entity_mask & required_mask) == required_mask) { /* 匹配 */ }
  ```
- **避免重复存储**：系统只读写统一数据池
- **多System共享组件**：Transform等组件可以被渲染、物理、碰撞系统同时使用

## 3. 结构示意

```
Entity(1)
  ├─ Transform  → RenderSystem / PhysicsSystem / CollisionSystem
  ├─ Sprite     → RenderSystem
  ├─ Velocity   → PhysicsSystem
  └─ Collider   → CollisionSystem

Entity(2)
  ├─ Transform  → RenderSystem
  └─ Sprite     → RenderSystem
```

## 4. 一行创建完整角色

通过工厂：
```cpp
uint32_t player = factory.create_player({100.0f, 200.0f});
```

通过模板：
```cpp
uint32_t player = templates::player().instantiate_at(&entity_mgr, {100.0f, 200.0f});
```

## 5. 建议的落地方式

1. **EntityManager统一组件管理**
2. **System只做逻辑**
3. **Factory封装创建**
4. **Template支持数据驱动**

## 6. 示例程序

完整示例程序见：
- [doc/ecs/ecs_example.cpp](ecs/ecs_example.cpp)
