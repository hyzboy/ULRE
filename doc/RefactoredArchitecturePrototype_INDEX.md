# 重构架构原型代码索引
## Refactored Architecture Prototype Index

根据 @hyzboy 的要求，创建了新的目录结构和空壳代码框架。

Created new directory structure and skeleton code framework as requested by @hyzboy.

---

## 文件清单 (File List)

### 核心接口 (Core Interfaces) - `inc/hgl/graph_v2/interface/`

1. **ISceneContext.h** (2030 字节)
   - 场景上下文接口
   - 纯场景容器，不涉及渲染
   - 提供节点注册和查询功能

2. **ITransformNode.h** (2446 字节)
   - 变换节点接口
   - 场景层次结构 + 坐标变换
   - 不直接依赖 World

3. **IComponentContainer.h** (2276 字节)
   - 组件容器接口
   - 解除 Component 与 SceneNode 的循环依赖
   - 只持有引用，不拥有生命周期

### 场景系统 (Scene System) - `inc/hgl/graph_v2/scene/`

4. **SceneContext.h** (2203 字节)
   - 场景上下文实现
   - 从 World 分离出的纯场景管理
   - 拥有节点注册表和根节点

5. **TransformNode.h** (4339 字节)
   - 变换节点实现（替代旧 SceneNode）
   - 实现 ITransformNode 和 IComponentContainer
   - 使用不同名称避免与旧系统冲突

6. **SceneManager.h** (2970 字节)
   - 场景管理器（从 RenderFramework 分离）
   - 管理多个场景上下文
   - 职责分离

### 渲染系统 (Render System) - `inc/hgl/graph_v2/render/`

7. **RenderContextV2.h** (3247 字节)
   - 增强的渲染上下文
   - 显式传递，不通过调用链
   - 轻量级临时对象

### 组件系统 (Component System) - `inc/hgl/component_v2/`

8. **Component.h** (7662 字节)
   - 简化的组件基类
   - 数据嵌入，移除 ComponentData 层
   - 单个类型哈希（从3个减少到1个）

9. **ComponentRegistry.h** (3122 字节)
   - 组件注册表（全局单例）
   - 替代通过 RenderFramework 管理
   - 独立的组件管理系统

10. **RenderComponentV2.h** (5351 字节)
    - 渲染组件实现示例
    - 展示如何使用新组件系统
    - 包含详细的使用示例

### 文档 (Documentation)

11. **README.md** (6137 字节)
    - 完整的架构说明
    - 使用示例
    - 迁移指南

---

## 关键特性 (Key Features)

### ✅ 命名隔离 (Namespace Isolation)
- `hgl::graph_v2` - 新场景系统
- `hgl::graph_v2::component` - 新组件系统
- 与旧系统完全隔离，可以共存

### ✅ 接口解耦 (Interface Decoupling)
```cpp
// 旧: TransformNode 依赖具体的 World
World* main_world;

// 新: TransformNode 依赖接口
ISceneContext* scene_context;
```

### ✅ 显式传递 (Explicit Passing)
```cpp
// 旧: 5层调用链
auto ctx = node->GetRenderContext();  // 脆弱

// 新: 显式参数
node->Render(ctx);  // 简单
```

### ✅ 简化类型系统 (Simplified Type System)
```cpp
// 旧: 3个类型哈希
Component::GetTypeHash()
Component::GetDataTypeHash()
Component::GetManagerTypeHash()

// 新: 1个类型哈希
Component::GetTypeHash()
```

### ✅ 职责分离 (Separation of Concerns)
```
旧系统:
RenderFramework (管理一切)
    ├─ 渲染资源
    ├─ 场景管理
    └─ 组件管理

新系统:
RenderFramework (只管理渲染资源)
SceneManager (只管理场景)
ComponentRegistry (只管理组件)
```

---

## 代码统计 (Code Statistics)

| 类型 | 文件数 | 总行数 | 说明 |
|------|--------|--------|------|
| 接口定义 | 3 | ~200行 | 核心抽象 |
| 场景实现 | 3 | ~300行 | 场景管理 |
| 渲染系统 | 1 | ~100行 | 渲染上下文 |
| 组件系统 | 3 | ~500行 | 组件框架 |
| 文档 | 1 | ~250行 | 使用说明 |
| **总计** | **11** | **~1350行** | **完整框架** |

---

## 架构对比 (Architecture Comparison)

### 依赖关系 (Dependencies)

**旧系统** (循环依赖):
```
SceneNode ←→ World ←→ RenderFramework
Component ←→ ComponentManager ←→ SceneNode
```

**新系统** (单向依赖):
```
TransformNode → ISceneContext
Component → ComponentManager
ComponentRegistry (独立)
```

### 调用复杂度 (Call Complexity)

| 操作 | 旧系统 | 新系统 | 改进 |
|------|--------|--------|------|
| 获取 RenderContext | 5层调用 | 直接参数 | 80%↓ |
| 创建组件 | 需要 Data | 直接创建 | 简化 |
| 类型检查 | 3个哈希 | 1个哈希 | 66%↓ |

---

## 使用示例 (Usage Examples)

### 创建场景
```cpp
// 创建场景上下文
auto* scene = new SceneContext("MyScene");

// 创建节点
auto* node = new TransformNode(scene);

// 建立层次
root->AddChild(node);
```

### 使用组件
```cpp
// 获取管理器
auto* mgr = GetComponentManager<RenderComponentV2Manager>();

// 创建组件
auto* comp = mgr->CreateComponent();

// 附加到节点
node->AttachComponent(comp);
```

### 渲染流程
```cpp
// 创建上下文
RenderContextV2 ctx;
ctx.scene_context = scene;
ctx.camera = camera;

// 渲染（显式传递）
root->Render(&ctx);
```

---

## 下一步 (Next Steps)

### 实现 .cpp 文件
1. SceneContext.cpp
2. TransformNode.cpp
3. Component.cpp
4. ComponentRegistry.cpp
5. SceneManager.cpp

### 集成到构建系统
1. 添加到 CMakeLists.txt
2. 创建测试用例
3. 编写示例程序

### 迁移准备
1. 创建适配器
2. 编写迁移工具
3. 制定迁移计划

---

## 参考文档 (References)

- [架构重构计划](../../doc/ArchitectureRefactoringPlan.md) - 完整的12步重构计划
- [架构图表](../../doc/ArchitectureRefactoringDiagrams.md) - 可视化架构对比
- [快速指南](../../doc/ArchitectureRefactoringPlan_README.md) - 快速开始
- [系统 README](./graph_v2/README.md) - 详细使用说明

---

## 文件位置 (File Locations)

所有文件位于:
- **头文件**: `/inc/hgl/graph_v2/` 和 `/inc/hgl/component_v2/`
- **源文件**: 待创建 (在 `/src/SceneGraph_v2/` 中)

---

**状态**: ✅ 框架完成  
**版本**: v0.1 (原型)  
**创建日期**: 2025-12-03  
**作者**: GitHub Copilot  
**审阅者**: @hyzboy
