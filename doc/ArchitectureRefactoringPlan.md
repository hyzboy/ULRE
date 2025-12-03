# ULRE 架构重构计划
## Architecture Refactoring Plan for SceneNode/World/SceneRenderer/RenderFramework/ComponentManager System

---

## 一、现状分析 (Current State Analysis)

### 1.1 主要问题识别 (Major Issues Identified)

#### 问题1: 紧耦合关系 (Tight Coupling)
当前架构中，核心类之间存在严重的紧耦合：
- **SceneNode** 依赖 World, RenderFramework, Component, ComponentManager
- **World** 依赖 RenderFramework, SceneNode
- **SceneRenderer** 依赖 World, RenderContext, RenderFramework
- **RenderFramework** 依赖所有其他类，包括 World, SceneRenderer, ComponentManager
- **Component** 依赖 SceneNode, ComponentManager, ComponentData
- **ComponentManager** 依赖 Component

**影响**: 修改任何一个类都会波及其他多个类，维护困难，测试困难。

#### 问题2: 循环依赖 (Circular Dependencies)
```
SceneNode ←→ World ←→ RenderFramework
Component ←→ ComponentManager ←→ SceneNode
SceneNode ←→ RenderContext (通过EventDispatcher链)
```

**影响**: 
- 编译依赖复杂，必须使用前向声明
- 难以理解对象创建和销毁顺序
- 容易出现悬空指针和内存泄漏

#### 问题3: 职责混乱 (Mixed Responsibilities)
- **RenderFramework** 既管理渲染资源（Texture, Material, Buffer），又管理组件（ComponentManager），还管理场景（World, SceneRenderer）
- **SceneNode** 既是场景层次结构节点，又是组件容器，还要处理坐标变换
- **World** 既管理节点，又管理事件分发，还管理UBO资源

**影响**: 类的职责不清晰，违反单一职责原则，难以扩展和维护。

#### 问题4: 所有权不明确 (Unclear Ownership)
- **Component** 被 SceneNode 和 ComponentManager 同时引用，但谁负责生命周期？
- **SceneNode** 在 World 的 all_nodes 中，但也可能在父节点的 child_nodes 中
- **CameraControl** 在 SceneRenderer 中创建，但所有权管理混乱

**影响**: 容易出现内存泄漏或重复释放。

#### 问题5: 脆弱的RenderContext获取 (Fragile RenderContext Access)
```cpp
RenderContext *SceneNode::GetRenderContext()const
{
    // 通过 World -> EventDispatcher -> Parent EventDispatcher 
    // -> dynamic_cast<SceneEventDispatcher> -> GetRenderContext()
    // 这个调用链太长且脆弱
}
```

**影响**: 
- 依赖EventDispatcher的父子关系链
- 需要运行时类型检查(dynamic_cast)
- 容易在某个环节失败返回nullptr

#### 问题6: ComponentData与Component分离的必要性存疑 (Questionable Component-Data Separation)
虽然注释说明了分离的理由（数据可被多个Component共享），但实际使用中：
- 大多数情况下是一对一关系
- 增加了类型系统的复杂度（三个hash: Component, Data, Manager）
- 没有明显的性能或架构优势

---

## 二、重构目标 (Refactoring Goals)

### 2.1 设计原则 (Design Principles)
1. **单一职责原则** (Single Responsibility Principle): 每个类只负责一个明确的职责
2. **依赖倒置原则** (Dependency Inversion Principle): 高层模块不应依赖低层模块，都应依赖抽象
3. **接口隔离原则** (Interface Segregation Principle): 客户端不应被迫依赖它不使用的接口
4. **最少知识原则** (Least Knowledge Principle): 减少对象间的依赖关系
5. **开闭原则** (Open-Closed Principle): 对扩展开放，对修改关闭

### 2.2 具体目标 (Specific Goals)
1. ✅ **解除循环依赖**: 通过接口和依赖注入解除循环依赖
2. ✅ **明确所有权**: 每个对象都有明确的所有者和生命周期管理
3. ✅ **职责分离**: 将混合的职责拆分到独立的类中
4. ✅ **简化RenderContext访问**: RenderContext应该是显式传递的，而不是通过复杂的链式调用
5. ✅ **降低耦合**: 通过抽象接口降低类之间的直接依赖
6. ✅ **可测试性**: 每个类都应该可以独立测试

---

## 三、重构步骤 (Refactoring Steps)

### 📋 Step 1: 创建核心接口定义 (Define Core Interfaces)
**目标**: 引入抽象接口，为解耦做准备

**文件变更**:
- 创建 `inc/hgl/graph/interface/ISceneContext.h` - 场景上下文接口
- 创建 `inc/hgl/graph/interface/IRenderContext.h` - 渲染上下文接口（增强现有）
- 创建 `inc/hgl/graph/interface/ITransformNode.h` - 变换节点接口
- 创建 `inc/hgl/graph/interface/IComponentContainer.h` - 组件容器接口

**接口定义**:
```cpp
// ISceneContext.h - 场景上下文接口
class ISceneContext 
{
public:
    virtual ~ISceneContext() = default;
    virtual const IDString& GetName() const = 0;
    virtual ITransformNode* GetRootNode() = 0;
};

// ITransformNode.h - 变换节点接口
class ITransformNode : public NodeTransform
{
public:
    virtual ~ITransformNode() = default;
    virtual ISceneContext* GetSceneContext() const = 0;
    virtual ITransformNode* GetParent() const = 0;
    virtual void AddChild(ITransformNode*) = 0;
};

// IComponentContainer.h - 组件容器接口
class IComponentContainer
{
public:
    virtual ~IComponentContainer() = default;
    virtual bool AttachComponent(Component*) = 0;
    virtual void DetachComponent(Component*) = 0;
    virtual const ComponentSet& GetComponents() const = 0;
};
```

**验证**: 编译通过，接口定义正确

---

### 📋 Step 2: 引入 SceneContext 类 (Introduce SceneContext)
**目标**: 创建独立的场景上下文类，替代World的部分职责

**文件变更**:
- 创建 `inc/hgl/graph/SceneContext.h`
- 创建 `src/SceneGraph/scene/SceneContext.cpp`

**SceneContext 职责**:
- 管理场景名称
- 管理场景根节点
- 管理场景内所有节点的注册表
- 提供节点查询功能

**变更内容**:
```cpp
// SceneContext.h
class SceneContext : public ISceneContext
{
    IDString context_name;
    ITransformNode* root_node = nullptr;
    Map<SceneNodeID, ITransformNode*> node_registry;
    
public:
    SceneContext(const IDString& name);
    virtual ~SceneContext();
    
    const IDString& GetName() const override { return context_name; }
    ITransformNode* GetRootNode() override { return root_node; }
    
    void RegisterNode(SceneNodeID id, ITransformNode* node);
    void UnregisterNode(SceneNodeID id);
    ITransformNode* FindNode(SceneNodeID id) const;
};
```

**迁移计划**:
- 将 World 的节点管理功能迁移到 SceneContext
- 保留 World 的渲染相关功能（UBO, DescriptorBinding）

**验证**: 编译通过，SceneContext可以创建和管理节点

---

### 📋 Step 3: 重构 ComponentManager 独立性 (Decouple ComponentManager)
**目标**: ComponentManager不再依赖RenderFramework，成为独立的管理器

**文件变更**:
- 修改 `inc/hgl/component/Component.h`
- 创建 `inc/hgl/component/ComponentRegistry.h`
- 创建 `src/SceneGraph/component/ComponentRegistry.cpp`

**ComponentRegistry 设计**:
```cpp
// ComponentRegistry.h - 组件管理器注册表
class ComponentRegistry
{
    static ComponentRegistry* instance;
    Map<size_t, ComponentManager*> manager_map;
    
public:
    static ComponentRegistry* Instance();
    
    bool RegisterManager(ComponentManager* mgr);
    bool UnregisterManager(size_t type_hash);
    ComponentManager* GetManager(size_t type_hash);
    
    template<typename T>
    T* GetManager(bool create_if_not_exist = true);
};
```

**变更内容**:
1. 移除 ComponentManager 的全局注册函数，改为使用 ComponentRegistry
2. 从 RenderFramework 中移除 ComponentManager 的直接管理
3. Component 通过 ComponentRegistry 获取 Manager，而不是通过 RenderFramework

**迁移计划**:
- 第一步：创建 ComponentRegistry，保留旧接口
- 第二步：修改所有 GetComponentManager 调用使用新接口
- 第三步：移除旧接口

**验证**: 
- 编译通过
- 组件创建和管理功能正常
- 不再需要 RenderFramework 来获取 ComponentManager

---

### 📋 Step 4: 优化 RenderContext 为一等公民 (Promote RenderContext)
**目标**: RenderContext成为显式传递的上下文对象，而不是通过复杂链式调用获取

**文件变更**:
- 增强 `inc/hgl/graph/RenderContext.h`
- 修改 `inc/hgl/graph/SceneNode.h`
- 修改 `src/SceneGraph/scene/SceneNode.cpp`

**设计变更**:
```cpp
// SceneNode.h - 移除脆弱的GetRenderContext()
class SceneNode : public ITransformNode, public IComponentContainer
{
    // 移除: RenderContext* GetRenderContext() const;
    // 改为在需要时显式传递 RenderContext
    
    // 渲染相关方法改为接受RenderContext参数
    virtual void Render(RenderContext* ctx);
    virtual void Update(RenderContext* ctx, double delta_time);
};

// RenderContext.h - 增强功能
class RenderContext
{
    World* world;
    Camera* camera;
    ViewportInfo* viewport;
    // ... 其他渲染状态
    
public:
    World* GetWorld() const { return world; }
    ISceneContext* GetSceneContext() const;
    // ... 其他访问器
};
```

**迁移计划**:
1. 在所有需要RenderContext的接口上添加参数
2. 修改调用点传递RenderContext
3. 移除通过EventDispatcher链获取RenderContext的代码

**验证**:
- 编译通过
- RenderContext可以正确传递到需要的地方
- 不再有dynamic_cast和复杂的调用链

---

### 📋 Step 5: 简化 SceneNode-World 关系 (Simplify SceneNode-World)
**目标**: SceneNode不直接依赖World，而是依赖SceneContext接口

**文件变更**:
- 修改 `inc/hgl/graph/SceneNode.h`
- 修改 `src/SceneGraph/scene/SceneNode.cpp`
- 修改 `inc/hgl/graph/World.h`

**变更内容**:
```cpp
// SceneNode.h
class SceneNode : public ITransformNode, public IComponentContainer
{
    ISceneContext* scene_context = nullptr;  // 改为接口
    // 移除: World* main_world = nullptr;
    
    // 移除: RenderFramework* GetRenderFramework() const;
    // 改为: ISceneContext* GetSceneContext() const { return scene_context; }
    
protected:
    void SetSceneContext(ISceneContext* ctx);
};

// World.h - World持有SceneContext
class World
{
    RenderFramework* render_framework;
    SceneContext* scene_context;  // 新增：场景上下文
    DescriptorBinding* world_desc_binding;
    // ...
};
```

**迁移计划**:
1. SceneNode 将 main_world 改为 scene_context
2. 所有 GetWorld() 调用改为 GetSceneContext()
3. World 内部创建并持有 SceneContext

**验证**:
- 编译通过
- SceneNode可以通过SceneContext访问场景信息
- 减少了直接依赖

---

### 📋 Step 6: 重构组件所有权和生命周期 (Refactor Component Ownership)
**目标**: 明确Component的所有权和生命周期管理

**设计决策**:
- **所有权模式**: ComponentManager 拥有 Component 的生命周期
- **引用模式**: SceneNode 只持有 Component 的弱引用或观察者
- **生命周期**: Component 由 ComponentManager 创建和销毁

**文件变更**:
- 修改 `inc/hgl/component/Component.h`
- 修改 `inc/hgl/graph/SceneNode.h`

**变更内容**:
```cpp
// Component.h
class Component
{
    uint unique_id;
    ComponentManager* manager;  // 拥有者
    
    // 移除: SceneNode* owner_node;  
    // 改为: 使用事件回调通知节点，但不持有引用
    
protected:
    virtual void OnAttachedToNode(IComponentContainer* container) {}
    virtual void OnDetachedFromNode(IComponentContainer* container) {}
};

// SceneNode.h
class SceneNode
{
    // Component 由 Manager 管理，SceneNode 只持有引用
    ComponentSet component_set;  // 不负责删除
    
    bool AttachComponent(Component* comp);
    void DetachComponent(Component* comp);
    // 析构函数中不删除 Component，只调用 DetachComponent
};

// ComponentManager.h
class ComponentManager
{
    // ComponentManager 负责 Component 的生命周期
    ComponentSet owned_components;  // 拥有所有权
    
    Component* CreateComponent(ComponentDataPtr data);
    void DestroyComponent(Component* comp);  // 新增：显式销毁
};
```

**迁移计划**:
1. 修改 Component 析构逻辑，从 SceneNode 移除引用
2. 修改 ComponentManager 添加 DestroyComponent 方法
3. 修改所有组件创建代码，确保通过 Manager 创建
4. 修改所有组件销毁代码，确保通过 Manager 销毁

**验证**:
- 使用Valgrind或AddressSanitizer检查内存泄漏
- 确认组件可以正确创建和销毁
- 确认SceneNode销毁时不会删除Component

---

### 📋 Step 7: 拆分 RenderFramework 职责 (Split RenderFramework Responsibilities)
**目标**: RenderFramework 只负责渲染框架初始化和资源管理，不管理场景和组件

**文件变更**:
- 修改 `inc/hgl/graph/RenderFramework.h`
- 创建 `inc/hgl/graph/SceneManager.h`
- 创建 `src/SceneGraph/scene/SceneManager.cpp`

**新类 SceneManager**:
```cpp
// SceneManager.h - 场景管理器
class SceneManager
{
    RenderFramework* render_framework;
    Map<IDString, World*> world_map;
    World* default_world = nullptr;
    
public:
    SceneManager(RenderFramework* rf);
    ~SceneManager();
    
    World* CreateWorld(const IDString& name);
    World* GetWorld(const IDString& name);
    void DestroyWorld(const IDString& name);
    
    World* GetDefaultWorld() { return default_world; }
    void SetDefaultWorld(World* world) { default_world = world; }
};
```

**RenderFramework 变更**:
```cpp
// RenderFramework.h
class RenderFramework
{
    // 保留：渲染资源管理
    TextureManager* tex_manager;
    MaterialManager* material_manager;
    BufferManager* buffer_manager;
    // ...
    
    // 移除：场景管理
    // World* default_world;  // 移动到 SceneManager
    // SceneRenderer* default_scene_renderer;  // 移动到 SceneManager
    
    // 移除：组件管理
    // CameraComponentManager* camera_component_manager;  // 移动到 ComponentRegistry
    // LightComponentManager* light_component_manager;
    
    // 新增：场景管理器
    SceneManager* scene_manager;
    
public:
    SceneManager* GetSceneManager() { return scene_manager; }
    
    // 便捷访问器（委托给SceneManager）
    World* GetDefaultWorld() { return scene_manager->GetDefaultWorld(); }
};
```

**迁移计划**:
1. 创建 SceneManager 类
2. 将场景管理相关代码从 RenderFramework 移动到 SceneManager
3. 修改所有访问 default_world 的代码
4. 移除 RenderFramework 中的 ComponentManager 管理

**验证**:
- 编译通过
- 场景创建和管理功能正常
- RenderFramework 职责更加清晰

---

### 📋 Step 8: 重新评估 ComponentData 的必要性 (Re-evaluate ComponentData)
**目标**: 简化或移除 ComponentData 层，减少不必要的抽象

**分析**:
```cpp
// 当前模式：三层结构
ComponentManager -> Component -> ComponentData

// 问题：
// 1. 大多数情况下 Component 和 ComponentData 是一对一的
// 2. 三个类型哈希（Manager, Component, Data）增加复杂度
// 3. ComponentData 的共享特性很少被使用
```

**两种方案**:

#### 方案A: 保留但简化
- 保留 ComponentData，但简化类型系统
- 移除多余的类型哈希
- 明确 Data 的共享语义

#### 方案B: 合并到 Component
- 将 ComponentData 的数据直接放到 Component 中
- 如果需要共享，使用 SharedPtr<Component>
- 大幅简化类型系统

**推荐**: 方案B - 合并到Component

**文件变更**:
- 修改 `inc/hgl/component/Component.h`
- 删除 `inc/hgl/component/ComponentData.h` 的独立定义
- 修改所有 Component 子类

**变更内容**:
```cpp
// Component.h - 简化后
class Component
{
    uint unique_id;
    ComponentManager* manager;
    
    // 数据直接作为成员，不需要 ComponentData
    // 子类根据需要添加自己的数据成员
    
public:
    Component(ComponentManager* mgr);
    virtual ~Component();
    
    // 只保留一个类型哈希
    virtual size_t GetTypeHash() const = 0;
};

// 使用宏简化
#define COMPONENT_CLASS(name) \
    static constexpr size_t StaticTypeHash() { return hgl::GetTypeHash<name##Component>(); } \
    virtual size_t GetTypeHash() const override { return StaticTypeHash(); }
```

**迁移计划**:
1. 分析所有现有 Component 和 ComponentData 的使用
2. 逐个 Component 将 Data 合并到 Component 中
3. 更新 ComponentManager 的创建接口
4. 移除 ComponentData 相关的类型系统

**验证**:
- 编译通过
- 所有组件功能正常
- 代码复杂度显著降低

---

### 📋 Step 9: 移除循环依赖 (Remove Circular Dependencies)
**目标**: 确保所有循环依赖已被解除

**检查清单**:
- [ ] SceneNode 不直接依赖 World（通过 ISceneContext）
- [ ] Component 不直接依赖 SceneNode（通过 IComponentContainer）
- [ ] ComponentManager 独立，不依赖 RenderFramework
- [ ] RenderContext 作为参数传递，不通过 EventDispatcher 链获取

**依赖图（重构后）**:
```
RenderFramework
    ↓
SceneManager → World → SceneContext → SceneNode
    ↓
RenderContext (作为参数传递)

ComponentRegistry → ComponentManager → Component
    ↑
SceneNode (可选依赖)
```

**验证工具**:
```bash
# 使用 include-what-you-use 检查头文件依赖
iwyu_tool.py -p . > iwyu.log

# 或使用自定义脚本检查循环依赖
python scripts/check_circular_deps.py
```

**验证**:
- 依赖图无环
- 可以独立编译每个模块
- 头文件包含关系清晰

---

### 📋 Step 10: 更新文档和示例 (Update Documentation)
**目标**: 更新所有相关文档，确保开发者理解新架构

**文档变更**:
- 创建 `doc/Architecture.md` - 新架构说明
- 更新 `README.md` - 反映架构变化
- 创建 `doc/ComponentSystem.md` - 组件系统使用指南
- 创建 `doc/MigrationGuide.md` - 从旧API迁移指南

**Architecture.md 内容**:
```markdown
# ULRE Architecture

## Core Concepts

### Scene Context
- SceneContext: 场景节点容器
- SceneNode: 场景层次结构节点
- World: 渲染世界（包含SceneContext和渲染资源）

### Component System
- Component: 组件基类
- ComponentManager: 组件管理器（拥有生命周期）
- ComponentRegistry: 全局组件管理器注册表

### Render System
- RenderFramework: 渲染框架（资源管理）
- RenderContext: 渲染上下文（临时状态）
- SceneRenderer: 场景渲染器

## Design Principles
- 单一职责
- 依赖倒置
- 明确所有权
- 低耦合高内聚
```

**示例代码更新**:
- 更新所有 example 目录下的示例代码
- 确保使用新API

**验证**:
- 文档完整且准确
- 示例代码可以编译运行
- 开发者可以理解新架构

---

### 📋 Step 11: 全面测试和验证 (Comprehensive Testing)
**目标**: 确保重构没有引入bug，所有功能正常

**测试计划**:

#### 单元测试
```cpp
// 测试 SceneContext
TEST(SceneContext, CreateAndManageNodes)
TEST(SceneContext, NodeRegistry)

// 测试 ComponentManager
TEST(ComponentManager, CreateComponent)
TEST(ComponentManager, ComponentLifecycle)

// 测试 SceneNode
TEST(SceneNode, AttachDetachComponent)
TEST(SceneNode, HierarchyManagement)
```

#### 集成测试
- 完整场景创建和销毁
- 组件添加和移除
- 渲染流程

#### 回归测试
- 运行所有现有example
- 确保渲染结果一致
- 性能测试（不应有显著降低）

**验证**:
- 所有测试通过
- 没有内存泄漏
- 性能符合预期

---

### 📋 Step 12: 代码审查和优化 (Code Review)
**目标**: 审查重构代码，进行必要的优化

**审查清单**:
- [ ] 所有类都有明确的职责
- [ ] 没有循环依赖
- [ ] 所有权和生命周期明确
- [ ] 接口设计合理
- [ ] 代码风格一致
- [ ] 注释和文档完整

**性能优化**:
- 检查是否有不必要的拷贝
- 检查是否有可以缓存的计算
- 检查是否有可以并行的操作

**验证**:
- 通过代码审查
- 性能测试符合预期
- 代码质量高

---

## 四、风险和缓解措施 (Risks and Mitigation)

### 4.1 风险识别

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| API不兼容导致现有代码无法编译 | 高 | 高 | 提供兼容层和迁移指南 |
| 重构引入新bug | 中 | 高 | 充分测试，每步都验证 |
| 性能下降 | 低 | 中 | 性能测试，优化关键路径 |
| 开发者学习成本 | 中 | 中 | 完善文档和示例 |
| 重构周期过长 | 中 | 中 | 分步进行，每步可编译测试 |

### 4.2 回滚计划
- 每个步骤都提交到版本控制
- 保持主分支稳定，在feature分支开发
- 如果某步失败，可以回滚到上一步

---

## 五、时间估算 (Time Estimation)

| 步骤 | 预计时间 | 优先级 |
|------|----------|--------|
| Step 1: 定义核心接口 | 2-3小时 | P0 |
| Step 2: 引入 SceneContext | 3-4小时 | P0 |
| Step 3: 重构 ComponentManager | 4-6小时 | P0 |
| Step 4: 优化 RenderContext | 3-4小时 | P0 |
| Step 5: 简化 SceneNode-World | 2-3小时 | P0 |
| Step 6: 重构组件所有权 | 4-6小时 | P1 |
| Step 7: 拆分 RenderFramework | 3-4小时 | P1 |
| Step 8: 重评估 ComponentData | 6-8小时 | P2 |
| Step 9: 移除循环依赖 | 2-3小时 | P0 |
| Step 10: 更新文档 | 4-6小时 | P1 |
| Step 11: 全面测试 | 6-8小时 | P0 |
| Step 12: 代码审查 | 2-3小时 | P1 |
| **总计** | **41-58小时** | |

---

## 六、成功标准 (Success Criteria)

### 6.1 功能标准
- ✅ 所有现有功能正常工作
- ✅ 所有example可以编译运行
- ✅ 渲染结果与重构前一致

### 6.2 架构标准
- ✅ 无循环依赖
- ✅ 每个类职责明确
- ✅ 所有权和生命周期清晰
- ✅ 接口设计合理

### 6.3 质量标准
- ✅ 无内存泄漏
- ✅ 无编译警告
- ✅ 代码风格一致
- ✅ 测试覆盖率 > 80%

### 6.4 文档标准
- ✅ 架构文档完整
- ✅ API文档更新
- ✅ 示例代码更新
- ✅ 迁移指南完整

---

## 七、后续改进 (Future Improvements)

### 7.1 短期改进（3个月内）
1. **性能优化**
   - 使用对象池减少分配
   - 批量处理组件更新
   - 缓存频繁计算的结果

2. **易用性改进**
   - 提供更多便捷API
   - 添加更多示例
   - 改进错误提示

### 7.2 中期改进（6个月内）
1. **序列化支持**
   - 场景序列化和反序列化
   - 组件序列化
   - 运行时资源热加载

2. **编辑器集成**
   - 可视化场景编辑
   - 组件拖拽添加
   - 实时预览

### 7.3 长期改进（1年内）
1. **多线程支持**
   - 并行更新组件
   - 并行渲染
   - 任务调度系统

2. **脚本绑定**
   - Lua/Python脚本支持
   - 脚本组件
   - 热重载

---

## 八、总结 (Conclusion)

本重构计划针对ULRE引擎当前SceneNode/World/SceneRenderer/RenderFramework/ComponentManager体系的设计问题，提出了系统化的解决方案。通过12个明确的步骤，逐步解耦各个模块，明确职责和所有权，最终实现一个低耦合、高内聚、易扩展的架构。

每个步骤都可以独立编译测试，降低了重构风险。同时，通过充分的文档和测试，确保重构的成功和代码质量。

**关键要点**:
1. 🎯 通过接口解耦，降低模块间的依赖
2. 🎯 明确所有权，避免内存管理问题
3. 🎯 单一职责，提高代码可维护性
4. 🎯 显式传递，避免复杂的调用链
5. 🎯 分步进行，每步都可验证

---

**文档版本**: v1.0  
**创建日期**: 2025-12-03  
**作者**: ULRE Architecture Team  
**状态**: 待审核
