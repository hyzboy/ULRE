# 架构重构图解
## Architecture Refactoring Diagrams

---

## 一、当前架构问题图示 (Current Architecture Issues)

### 1.1 类依赖关系图 (Class Dependency Graph)

```
                    [RenderFramework]
                     /      |      \
                    /       |       \
                   /        |        \
              [World]  [SceneRenderer]  [ComponentManager]
               /   \         |              /    \
              /     \        |             /      \
      [SceneNode]    \       |       [Component]   \
          |   \       \      |           |          \
          |    \       \     |           |           \
          |  [Component] --> RenderContext  [ComponentData]
          |         \         |
          |          \________/
          |                   
    [Child SceneNode]

问题标记：
━━━  直接依赖
- - - 通过 EventDispatcher 链获取
~~~~~ 循环依赖
```

#### 循环依赖详解

```
循环1: SceneNode ←→ World
    SceneNode.main_world -> World
    World.root_node -> SceneNode
    World.all_nodes -> SceneNode

循环2: World ←→ RenderFramework  
    World.render_framework -> RenderFramework
    RenderFramework.default_world -> World

循环3: SceneNode ←→ Component
    SceneNode.component_set -> Component
    Component.OwnerNode -> SceneNode

循环4: Component ←→ ComponentManager
    Component.Manager -> ComponentManager
    ComponentManager.component_set -> Component
```

---

### 1.2 职责混乱图 (Mixed Responsibilities)

```
┌─────────────────────────────────────────────────────┐
│              RenderFramework                        │
├─────────────────────────────────────────────────────┤
│ [渲染资源管理]                                        │
│  - TextureManager                                   │
│  - MaterialManager                                  │
│  - BufferManager                                    │
├─────────────────────────────────────────────────────┤
│ [场景管理] ⚠️ 职责混乱                                │
│  - default_world                                    │
│  - default_scene_renderer                           │
├─────────────────────────────────────────────────────┤
│ [组件管理] ⚠️ 职责混乱                                │
│  - camera_component_manager                         │
│  - light_component_manager                          │
├─────────────────────────────────────────────────────┤
│ [模块管理]                                           │
│  - module_manager                                   │
└─────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│                  SceneNode                          │
├─────────────────────────────────────────────────────┤
│ [场景层次] ✓ 合理                                     │
│  - parent_node                                      │
│  - child_nodes                                      │
├─────────────────────────────────────────────────────┤
│ [坐标变换] ✓ 合理                                     │
│  - NodeTransform                                    │
├─────────────────────────────────────────────────────┤
│ [组件容器] ⚠️ 职责过多                                │
│  - component_set                                    │
├─────────────────────────────────────────────────────┤
│ [渲染相关] ⚠️ 耦合过紧                                │
│  - GetRenderContext() [通过EventDispatcher链]       │
│  - GetRenderFramework() [通过World]                │
└─────────────────────────────────────────────────────┘
```

---

### 1.3 脆弱的 RenderContext 获取链 (Fragile RenderContext Access Chain)

```
调用: SceneNode::GetRenderContext()
    ↓
获取 World: this->main_world
    ↓
获取 EventDispatcher: world->GetEventDispatcher()
    ↓
获取父 EventDispatcher: ed->GetParent()
    ↓
dynamic_cast: dynamic_cast<SceneEventDispatcher*>(parent_ed)
    ↓
获取 RenderContext: sep->GetRenderContext()

问题：
❌ 调用链太长（5层）
❌ 依赖 EventDispatcher 的父子关系
❌ 需要运行时类型检查（dynamic_cast）
❌ 任何环节失败都会返回 nullptr
❌ 难以调试和维护
```

---

## 二、重构后架构图示 (Refactored Architecture)

### 2.1 解耦后的依赖关系 (Decoupled Dependencies)

```
[RenderFramework]
     |
     ├─── [ResourceManagers] (TextureManager, MaterialManager, etc.)
     |
     └─── [SceneManager]
              |
              └─── [World]
                      |
                      ├─── [SceneContext] ──┐
                      |         |           |
                      |         └─→ [SceneNode Tree]
                      |                |
                      |                └─→ [SceneNode]
                      |
                      └─── [RenderResources] (UBO, DescriptorBinding)


[ComponentRegistry] (全局单例)
     |
     └─── [ComponentManager]
              |
              └─── [Component] (包含数据)


[RenderContext] (作为参数显式传递)
     ├─ world
     ├─ camera
     └─ viewport

特点：
✓ 无循环依赖
✓ 单向依赖流
✓ 明确的层次结构
```

---

### 2.2 职责分离后的类结构 (Separated Responsibilities)

```
┌──────────────────────────┐
│   RenderFramework        │  [渲染框架]
├──────────────────────────┤
│ ✓ 渲染资源管理            │  - 只负责渲染相关资源
│ ✓ Vulkan 设备管理        │
│ ✓ 资源 Manager 管理      │
└──────────────────────────┘

┌──────────────────────────┐
│   SceneManager           │  [场景管理器]
├──────────────────────────┤
│ ✓ World 创建和管理       │  - 从 RenderFramework 分离
│ ✓ SceneRenderer 管理     │
└──────────────────────────┘

┌──────────────────────────┐
│   World                  │  [渲染世界]
├──────────────────────────┤
│ ✓ SceneContext 持有      │  - 职责更清晰
│ ✓ 渲染资源 (UBO, Desc)   │
│ ✓ EventDispatcher        │
└──────────────────────────┘

┌──────────────────────────┐
│   SceneContext           │  [场景容器] (新增)
├──────────────────────────┤
│ ✓ 节点注册表             │  - 纯场景管理
│ ✓ 根节点                │  - 不涉及渲染
│ ✓ 节点查询              │
└──────────────────────────┘

┌──────────────────────────┐
│   SceneNode              │  [场景节点]
├──────────────────────────┤
│ ✓ 层次结构管理           │  - 职责简化
│ ✓ 坐标变换              │  - 不直接访问渲染
│ ✓ 组件容器              │
└──────────────────────────┘

┌──────────────────────────┐
│   ComponentRegistry      │  [组件注册表] (新增)
├──────────────────────────┤
│ ✓ Manager 全局注册       │  - 独立的组件系统
│ ✓ Manager 查询          │
└──────────────────────────┘

┌──────────────────────────┐
│   ComponentManager       │  [组件管理器]
├──────────────────────────┤
│ ✓ Component 创建         │  - 拥有 Component 生命周期
│ ✓ Component 销毁         │  - 不依赖 RenderFramework
│ ✓ Component 更新         │
└──────────────────────────┘

┌──────────────────────────┐
│   Component              │  [组件]
├──────────────────────────┤
│ ✓ 组件逻辑               │  - 简化：直接包含数据
│ ✓ 数据存储               │  - 不需要 ComponentData 层
└──────────────────────────┘
```

---

### 2.3 接口层次结构 (Interface Hierarchy)

```
                [ISceneContext]
                       ↑
                       |
                 [SceneContext]


[ITransformNode]            [IComponentContainer]
       ↑                             ↑
       |                             |
       └──────────┬──────────────────┘
                  |
              [SceneNode]


[IRenderContext]
       ↑
       |
  [RenderContext]
```

---

### 2.4 对象所有权图 (Object Ownership Graph)

```
RenderFramework (owns)
    └─→ SceneManager (owns)
            └─→ World[] (owns)
                    ├─→ SceneContext (owns)
                    |       └─→ SceneNode Tree (owns)
                    |               └─→ SceneNode (refs Component)
                    |
                    └─→ RenderResources (owns)


ComponentRegistry (singleton)
    └─→ ComponentManager[] (owns)
            └─→ Component[] (owns)
                    └─→ data (embedded)


SceneRenderer (refs)
    ├─→ World (ref)
    ├─→ RenderContext (owns)
    └─→ CameraControl (owns)

图例:
(owns)  - 拥有所有权，负责销毁
(refs)  - 引用，不负责销毁
(embedded) - 嵌入数据，随对象销毁
```

---

### 2.5 RenderContext 传递模式 (RenderContext Passing Pattern)

#### 重构前（脆弱）
```
SceneRenderer::RenderFrame()
    └─→ calls SceneNode::Render()
            └─→ calls GetRenderContext()  ❌ 脆弱的链式调用
                    └─→ ... (复杂的调用链)
```

#### 重构后（显式）
```
SceneRenderer::RenderFrame()
    |
    ├─→ creates/updates RenderContext ctx
    |
    └─→ calls SceneNode::Render(ctx)  ✓ 显式传递
            └─→ uses ctx directly
                    └─→ calls child->Render(ctx)  ✓ 继续传递
```

---

## 三、组件系统演化 (Component System Evolution)

### 3.1 当前组件系统 (Current Component System)

```
┌─────────────────────────────────────────────┐
│            ComponentData                    │
│  - 数据载体                                  │
│  - 可被多个 Component 共享（理论上）          │
│  - 3个类型哈希 (Data, Component, Manager)   │
└─────────────────────────────────────────────┘
                    ↑
                    | SharedPtr
                    |
┌─────────────────────────────────────────────┐
│            Component                        │
│  - 逻辑和行为                                │
│  - 引用 ComponentData                       │
│  - 引用 SceneNode (OwnerNode)               │
│  - 引用 ComponentManager                    │
└─────────────────────────────────────────────┘
                    ↑
                    | raw pointer
                    |
┌─────────────────────────────────────────────┐
│         ComponentManager                    │
│  - 管理 Component 集合                       │
│  - CreateComponent(ComponentDataPtr)        │
└─────────────────────────────────────────────┘

问题：
❌ 三层结构复杂
❌ Data 共享很少使用
❌ 所有权不明确（SceneNode 和 Manager 都引用 Component）
❌ 3个类型哈希增加复杂度
```

---

### 3.2 重构后组件系统 (Refactored Component System)

#### 方案A: 保留但简化
```
┌─────────────────────────────────────────────┐
│            ComponentData                    │
│  - 纯数据结构                                │
│  - 可选的共享语义                            │
│  - 1个类型哈希                               │
└─────────────────────────────────────────────┘
                    ↑
                    | optional SharedPtr
                    |
┌─────────────────────────────────────────────┐
│            Component                        │
│  - 逻辑 + 数据                               │
│  - 不引用 SceneNode                          │
│  - 由 Manager 拥有                           │
│  - 1个类型哈希                               │
└─────────────────────────────────────────────┘
```

#### 方案B: 合并（推荐）
```
┌─────────────────────────────────────────────┐
│            Component                        │
│  - 逻辑 + 数据合并                           │
│  - 数据直接作为成员                          │
│  - 不引用 SceneNode                          │
│  - 由 Manager 拥有                           │
│  - 1个类型哈希                               │
│                                              │
│  示例:                                       │
│  class RenderComponent : public Component   │
│  {                                           │
│      Primitive* primitive;  // 数据成员      │
│      Material* material;    // 数据成员      │
│      // ... 其他数据                         │
│  };                                          │
└─────────────────────────────────────────────┘

优势：
✓ 结构简单
✓ 类型系统简化
✓ 所有权明确
✓ 学习成本低
✓ 如需共享，使用 SharedPtr<Component>
```

---

## 四、数据流对比 (Data Flow Comparison)

### 4.1 渲染数据流 - 重构前

```
Application
    ↓
RenderFramework::Tick()
    ↓
SceneRenderer::RenderFrame()
    ↓
遍历 World->GetRootNode()->GetChildNode()
    ↓
SceneNode::GetRenderContext()  ❌ 脆弱调用链
    ├─→ GetWorld()
    ├─→ world->GetEventDispatcher()->GetParent()
    └─→ dynamic_cast<SceneEventDispatcher*>()->GetRenderContext()
    ↓
使用 RenderContext 渲染
```

---

### 4.2 渲染数据流 - 重构后

```
Application
    ↓
RenderFramework::Tick()
    ↓
SceneManager::GetDefaultWorld()
    ↓
SceneRenderer::RenderFrame()
    ↓
创建/更新 RenderContext ctx
    ↓
遍历 World->GetSceneContext()->GetRootNode()
    ↓
调用 SceneNode::Render(ctx)  ✓ 显式传递
    ↓
使用 ctx 渲染，递归调用子节点
```

---

## 五、迁移路径示意图 (Migration Path)

```
当前状态
    ↓
Step 1-2: 引入接口和 SceneContext
    ↓
Step 3-4: 解耦 ComponentManager 和 RenderContext
    ↓
Step 5: 简化 SceneNode-World 关系
    ↓
Step 6-7: 明确所有权，拆分 RenderFramework
    ↓
Step 8: 简化组件系统
    ↓
Step 9: 验证无循环依赖
    ↓
Step 10-12: 完善文档和测试
    ↓
目标状态：低耦合、高内聚、易扩展
```

每个步骤后都应该：
✅ 编译通过
✅ 测试通过
✅ 提交到版本控制

---

## 六、关键改进总结 (Key Improvements Summary)

### 解耦改进
```
依赖关系数量:
  重构前: ~20个直接依赖关系
  重构后: ~10个直接依赖关系
  改进: 50% 减少

循环依赖:
  重构前: 4个循环依赖
  重构后: 0个循环依赖
  改进: 100% 消除
```

### 职责改进
```
RenderFramework 职责数:
  重构前: 5个职责（资源、场景、组件、模块、事件）
  重构后: 2个职责（资源、模块）
  改进: 60% 减少

SceneNode 耦合度:
  重构前: 依赖 5个类（World, RenderFramework, Component, etc.）
  重构后: 依赖 2个接口（ISceneContext, IComponentContainer）
  改进: 60% 减少
```

### 可维护性改进
```
调用链深度:
  RenderContext 获取:
    重构前: 5层调用链
    重构后: 1层（参数传递）
    改进: 80% 减少

类型系统复杂度:
  Component 类型哈希:
    重构前: 3个哈希（Component, Data, Manager）
    重构后: 1个哈希（Component）
    改进: 66% 减少
```

---

**文档版本**: v1.0  
**创建日期**: 2025-12-03  
**图表说明**: 使用 ASCII 艺术图表，便于在任何终端和编辑器中查看
