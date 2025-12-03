# 重构架构原型代码 (Refactored Architecture Prototype)

这是基于[架构重构计划](../../doc/ArchitectureRefactoringPlan.md)创建的原型代码框架。

This is the prototype code framework based on the [Architecture Refactoring Plan](../../doc/ArchitectureRefactoringPlan.md).

---

## 目录结构 (Directory Structure)

```
inc/hgl/graph_v2/           # 新的场景图系统
    ├── interface/          # 核心接口定义
    │   ├── ISceneContext.h         # 场景上下文接口
    │   ├── ITransformNode.h        # 变换节点接口
    │   └── IComponentContainer.h   # 组件容器接口
    │
    ├── scene/              # 场景相关实现
    │   ├── SceneContext.h          # 场景上下文实现
    │   ├── TransformNode.h         # 变换节点实现
    │   └── SceneManager.h          # 场景管理器
    │
    └── render/             # 渲染相关
        └── RenderContextV2.h       # 增强的渲染上下文

inc/hgl/component_v2/       # 新的组件系统
    ├── Component.h                 # 简化的组件基类
    └── ComponentRegistry.h         # 组件注册表
```

---

## 命名约定 (Naming Convention)

为了与旧系统区分并便于迁移，新系统使用以下命名：

| 旧系统 (Legacy) | 新系统 (V2) | 说明 |
|----------------|-------------|------|
| `hgl::graph` | `hgl::graph_v2` | 新命名空间 |
| `hgl::component` | `hgl::graph_v2::component` | 组件命名空间 |
| `SceneNode` | `TransformNode` | 避免冲突 |
| `World` | `SceneContext` + `SceneManager` | 职责分离 |
| `RenderContext` | `RenderContextV2` | 增强版本 |
| `Component` + `ComponentData` | `Component` (合并) | 简化 |

---

## 核心设计变更 (Core Design Changes)

### 1. 接口解耦 (Interface Decoupling)

**旧系统**:
```cpp
// SceneNode 直接依赖 World
class SceneNode {
    World* main_world;  // 紧耦合
};
```

**新系统**:
```cpp
// TransformNode 依赖接口
class TransformNode : public ITransformNode {
    ISceneContext* scene_context;  // 接口，低耦合
};
```

### 2. RenderContext 显式传递 (Explicit RenderContext Passing)

**旧系统**:
```cpp
// 5层调用链，脆弱
RenderContext* SceneNode::GetRenderContext() const {
    auto world = GetWorld();
    auto ed = world->GetEventDispatcher()->GetParent();
    auto sep = dynamic_cast<SceneEventDispatcher*>(ed);
    return sep->GetRenderContext();  // 可能失败
}
```

**新系统**:
```cpp
// 显式传递，简单可靠
void TransformNode::Render(RenderContextV2* ctx) {
    // 直接使用
    auto camera = ctx->camera;
    // 传递给子节点
    for (auto child : children)
        child->Render(ctx);
}
```

### 3. 组件系统简化 (Component System Simplification)

**旧系统**:
```cpp
// 三层结构
ComponentData (数据)
    ↑
Component (逻辑)
    ↑
ComponentManager (管理)

// 3个类型哈希
GetTypeHash()           // Component
GetDataTypeHash()       // ComponentData
GetManagerTypeHash()    // ComponentManager
```

**新系统**:
```cpp
// 单层结构
Component (逻辑 + 数据)
    ↑
ComponentManager (管理)

// 1个类型哈希
GetTypeHash()  // Component
```

### 4. 职责分离 (Responsibility Separation)

**旧系统**:
```cpp
class RenderFramework {
    // 混合职责
    TextureManager* tex_manager;        // 渲染资源
    World* default_world;               // 场景管理
    ComponentManager* comp_manager;     // 组件管理
};
```

**新系统**:
```cpp
// 分离职责
class RenderFrameworkV2 {
    TextureManager* tex_manager;  // 只管理渲染资源
};

class SceneManager {
    SceneContext* scenes[];  // 只管理场景
};

class ComponentRegistry {
    ComponentManager* managers[];  // 全局管理组件
};
```

---

## 使用示例 (Usage Examples)

### 示例 1: 创建场景和节点

```cpp
// 1. 创建场景上下文
SceneContext* scene = new SceneContext("MyScene");

// 2. 创建节点
TransformNode* root = new TransformNode(scene);
TransformNode* child = new TransformNode(scene);

// 3. 建立层次关系
root->AddChild(child);

// 4. 设置变换
child->SetLocalPosition(Vector3f(1, 2, 3));
child->UpdateWorldTransform();
```

### 示例 2: 使用组件

```cpp
// 1. 获取组件管理器（自动创建）
auto manager = GetComponentManager<RenderComponentManager>();

// 2. 创建组件
RenderComponent* comp = manager->CreateComponent();

// 3. 附加到节点
node->AttachComponent(comp);

// 4. 销毁时
manager->DestroyComponent(comp);  // 由管理器负责
```

### 示例 3: 渲染流程

```cpp
void SceneRenderer::RenderFrame()
{
    // 1. 创建渲染上下文
    RenderContextV2 ctx;
    ctx.scene_context = scene_context;
    ctx.camera = camera;
    ctx.viewport = viewport;
    ctx.UpdateCameraInfo();
    
    // 2. 开始渲染
    BeginRender();
    
    // 3. 渲染根节点（显式传递上下文）
    auto root = ctx.GetRootNode();
    if (root)
        root->Render(&ctx);  // 简单！
    
    // 4. 提交
    Submit();
}
```

---

## 迁移步骤 (Migration Steps)

### 阶段 1: 共存期 (Coexistence Phase)

1. 新代码使用 `graph_v2` 命名空间
2. 旧代码继续使用 `graph` 命名空间
3. 两套系统可以同时编译

### 阶段 2: 逐步迁移 (Gradual Migration)

1. 新功能使用新系统
2. 逐个模块从旧系统迁移
3. 使用适配器连接新旧系统

### 阶段 3: 完全替换 (Complete Replacement)

1. 所有代码迁移到新系统
2. 移除旧系统代码
3. 重命名 `graph_v2` 为 `graph`

---

## 关键优势 (Key Advantages)

### 1. 解除循环依赖

```
旧系统:
SceneNode ←→ World ←→ RenderFramework

新系统:
TransformNode → ISceneContext (单向)
```

### 2. 降低复杂度

| 指标 | 旧系统 | 新系统 | 改进 |
|------|--------|--------|------|
| 调用链深度 | 5层 | 1层 | ↓ 80% |
| 类型哈希数 | 3个 | 1个 | ↓ 66% |
| 依赖关系 | ~20个 | ~10个 | ↓ 50% |

### 3. 提高可测试性

```cpp
// 新系统：可以独立测试
void TestTransformNode() {
    MockSceneContext mock_scene;
    TransformNode node(&mock_scene);
    // 测试节点功能，无需真实 World
}
```

### 4. 更清晰的所有权

```
新系统所有权关系:
SceneContext owns TransformNode
ComponentManager owns Component
TransformNode refs Component (不拥有)
```

---

## 待实现功能 (TODO)

当前代码只包含头文件框架，需要实现：

### 高优先级 (High Priority)
- [ ] SceneContext 的 .cpp 实现
- [ ] TransformNode 的 .cpp 实现
- [ ] Component 的 .cpp 实现
- [ ] ComponentRegistry 的 .cpp 实现

### 中优先级 (Medium Priority)
- [ ] SceneManager 的 .cpp 实现
- [ ] 节点遍历功能
- [ ] 组件查询优化
- [ ] 事件系统集成

### 低优先级 (Low Priority)
- [ ] 单元测试
- [ ] 性能测试
- [ ] 示例程序
- [ ] 迁移工具

---

## 编译说明 (Build Instructions)

当前代码为头文件框架，不会影响现有编译。

要启用新系统，需要：

1. 实现 .cpp 文件
2. 在 CMakeLists.txt 中添加源文件
3. 链接到目标

```cmake
# CMakeLists.txt 示例
set(GRAPH_V2_SOURCES
    src/SceneGraph_v2/scene/SceneContext.cpp
    src/SceneGraph_v2/scene/TransformNode.cpp
    src/SceneGraph_v2/scene/SceneManager.cpp
    src/SceneGraph_v2/component/Component.cpp
    src/SceneGraph_v2/component/ComponentRegistry.cpp
)

add_library(ULRE.SceneGraph_v2 ${GRAPH_V2_SOURCES})
```

---

## 参考文档 (References)

- [完整重构计划](../../doc/ArchitectureRefactoringPlan.md)
- [架构图表](../../doc/ArchitectureRefactoringDiagrams.md)
- [快速开始指南](../../doc/ArchitectureRefactoringPlan_README.md)

---

## 联系方式 (Contact)

如有问题或建议，请在项目中提 Issue 或 PR。

**状态**: 🚧 原型阶段 (Prototype Phase)  
**版本**: v0.1  
**创建日期**: 2025-12-03
