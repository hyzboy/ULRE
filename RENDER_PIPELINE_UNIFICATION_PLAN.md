# RenderPipeline 统一架构重构计划

**版本**: 1.0  
**创建日期**: 2026-02-23  
**优先级**: 高  
**影响范围**: 整个 ECS 渲染子系统  

---

## 目录

1. [现状分析](#现状分析)
2. [设计目标](#设计目标)
3. [设计原则](#设计原则)
4. [架构设计](#架构设计)
5. [具体迁移步骤](#具体迁移步骤)
6. [代码示例](#代码示例)
7. [Context 接口清理](#context-接口清理)
8. [时间表](#时间表)
9. [风险评估](#风险评估)
10. [验收标准](#验收标准)

---

## 现状分析

### 1.1 当前架构的问题

#### 问题 1: 混乱的管道实现模式

**Primitive 渲染** (接近正确的模式):
```
ECSContext::GetPrimitiveBatchPipeline() 
├─ 返回 PrimitiveBatchPipeline*（Pipeline 是独立的）
└─ 被 4 个"薄"System 分别调用（System 只是委托）:
   ├─ RenderPrimitiveCullSystem::Update() → pipeline->RunCull()
   ├─ RenderPrimitiveSortSystem::Update() → pipeline->RunSort()
   ├─ RenderPrimitiveBatchBuildSystem::Update() → pipeline->RunBuild()
   └─ RenderPrimitiveBatchFinalizeSystem::Update()
```

**Text 渲染** (接近正确的模式):
```
ECSContext::GetTextRenderPipeline()
├─ 返回 TextRenderPipeline*（Pipeline 是独立的）
└─ 被 3 个"薄"System 分别调用（System 只是委托）:
   ├─ TextCollectSystem::Update() → pipeline->RunCollect()
   ├─ TextResourceSyncSystem::Update() → pipeline->RunSync()
   └─ TextBuildSystem::Update() → pipeline->RunBuild()
```

**Line 渲染** (❌ 错误的反面教材!):
```
LineRenderSystem (既是 System，又是 Pipeline 所有者)
├─ 自身就是 System，注册到 ExecutionPhase
├─ 持有 LineRenderManager* （业务逻辑容器）
├─ Update() 在 RenderBatch 阶段做 Pipeline 工作(数据收集、批处理)
├─ Render() 在 RenderDrawSubmit 阶段做绘制
├─ 问题 A: System 不应该掺杂业务逻辑
├─ 问题 B: 没有独立的 Pipeline，无法复用
├─ 问题 C: 框架需要特殊认识 LineRenderSystem 这个类型
└─ 问题 D: 扩展性极差,每个新元素都这样搞
```

**Quad/Billboard 渲染** (❌ 同样错误):
```
QuadResourcePrepareSystem + QuadRenderSystem
├─ 混合了 System 和 Pipeline 的职责
├─ 自己持有资源管理逻辑
├─ 框架需要特殊认识这些 System
└─ 无法与其他元素统一管理
```

**现象**: 有两种截然不同的实现模式:
- **好的**: Primitive/Text - System(委托) → Pipeline(逻辑)
- **坏的**: Line/Quad - System ≈ Pipeline(混合体)

这导致代码风格不一致、难于维护和扩展。

#### 问题 2: Context 中的特定元素耦合

```cpp
class ECSContext {
private:
    std::unique_ptr<PrimitiveBatchPipeline> primitive_batch_pipeline;    // 针对 Primitive
    std::unique_ptr<TextRenderPipeline> text_render_pipeline;            // 针对 Text
    // Line 没有专门的 getter，混在 LineRenderSystem 中
    // Quad 没有专门的 getter，混在 QuadResourcePrepareSystem 中
    
public:
    PrimitiveBatchPipeline* GetPrimitiveBatchPipeline();   // 特定元素 API
    TextRenderPipeline* GetTextRenderPipeline();           // 特定元素 API
    // 缺少 GetLineRenderPipeline(), GetQuadRenderPipeline()
};
```

**问题**: 
- 每加一种渲染元素都要在 Context 中新增专门的 getter
- 无法体现 "所有渲染管道都是平等的" 这一设计理念
- 代码难以演进和扩展

#### 问题 3: System 与 Pipeline 的关系不清晰

**当前混乱的模式**:

```
模式 A: Primitive/Text
  System (薄代理) → Pipeline (业务逻辑)
  ✓ 清晰，但不一致
  
模式 B: Line/Quad  
  System ≈ Pipeline (混合体)
  ✗ 混淆了职责，难以维护
  ✗ 每个这样的 System 都是一个独特的雪花，难以标准化
  ✗ 无法通过框架统一管理
```

**问题**:
- **Primitive/Text**: System 是 Pipeline 的客户端，Pipeline 是共享资源（中间人模式）
- **Line/Quad**: System 本身就包含 Pipeline 逻辑（紧耦合），不该存在！
- 框架无法以统一的方式处理所有元素

**根本原因**:
LineRenderSystem 和 QuadRenderSystem 不应该存在！它们应该被拆分为：
- **LineRenderPipeline**: 业务逻辑容器
- **LineCollectSystem**: 薄代理，仅调用 pipeline->RunCollect()
- **LineBatchSystem**: 薄代理，仅调用 pipeline->RunBuild()
- **LineRenderSystem**: 薄代理，仅调用 pipeline->Render(cmd)

#### 问题 4: SystemGroup 与 Pipeline 没有显式关联

```cpp
// SystemGroup 只是一个元数据容器
struct SystemGroup {
    std::string name;              // "Primitive" / "Text" / "Line"
    ExecutionPhase startPhase;
    ExecutionPhase endPhase;
    bool enabled;
};

// Pipeline 是一个完全独立的对象，和 SystemGroup 没有显式关联
// 导致：
// - 启用 "Primitive" group 时，Pipeline 可能未初始化
// - 禁用 "Primitive" group 时，Pipeline 没有被清理
// - 无法通过 group.enabled 来判断 pipeline 的状态
```

---

## 设计目标

### 2.1 主要目标

**目标 1: 统一所有渲染管道的实现模式**
- Primitive、Text、Line、Quad 等所有渲染元素都通过 `RenderPipelineBase` 派生类实现
- 每种渲染元素对应一个 Pipeline 派生类和一个 SystemGroup
- 所有 Pipeline 的接口和生命周期保持一致

**目标 2: Context 接口隔离**
- 从 `ECSContext` 中完全移除针对特定元素的 getter（如 `GetPrimitiveBatchPipeline()`, `GetTextRenderPipeline()`）
- 所有 Pipeline 访问都通过统一的 `GetRenderPipeline(name)` 接口
- Context 不应该知道任何特定的 Pipeline 实现细节

**目标 3: System 与 Pipeline 的清晰分工**
- **System**: 调度器，驱动 Pipeline 各阶段按时执行
- **Pipeline**: 数据容器和算法容器，维护渲染状态和逻辑
- System 不直接操作渲染数据，全部委托给 Pipeline

**目标 4: SystemGroup 与 Pipeline 一一对应**
- 每个 SystemGroup 关联一个 Pipeline
- SystemGroup 的启用/禁用直接影响 Pipeline 的生命周期
- Pipeline 的实例化和清理由 SystemGroup 的安装器控制

**目标 5: 消除混合体 System - System 必须是"薄代理"**
- 不应该存在 `LineRenderSystem`、`QuadRenderSystem` 这样既是 System 又包含业务逻辑的混合体
- 所有 System 都应该是"薄代理"：仅在特定 Phase 调用 Pipeline 的对应方法
- 例如，`LineRenderSystem` 应该被拆分为：
  - `LineRenderPipeline` - 包含所有业务逻辑（数据收集、批处理、绘制命令）
  - `LineCollectSystem` - 薄代理，仅调用 `LineRenderPipeline::RunCollect()`
  - `LineBatchSystem` - 薄代理，仅调用 `LineRenderPipeline::RunBuild()`
  - `LineDrawSystem` - 薄代理，仅调用 `LineRenderPipeline::Render(cmd)`

### 2.2 次要目标

- 提高代码的可测试性：Pipeline 可独立单元测试
- 降低新增渲染类型的成本：只需新建 Pipeline + System，不需改 Context
- 改善代码的可读性和可维护性
- 为未来的动态加载插件预留扩展空间

---

## 设计原则

### 3.1 职责单一原则 (Single Responsibility)

**System** 只负责：
- 在特定 ExecutionPhase 被调用
- 调用 Pipeline 的相应方法

**Pipeline** 只负责：
- 维护渲染数据的收集、批处理、排序等逻辑
- 提供统一的多阶段接口

**Context** 只负责：
- 管理 System 的生命周期和执行顺序
- 统一的 Pipeline 注册/查询接口
- 不应该知道任何 Pipeline 的具体实现

### 3.2 开闭原则 (Open-Closed)

**对扩展开放**: 新增一种渲染元素时，只需：
1. 创建新的 Pipeline 派生类
2. 创建对应的 System
3. 创建 SystemGroup 安装器
4. 无需修改 Context、现有 Pipeline 或 System

**对修改关闭**: 不应该修改现有 Context 接口、RenderPipelineBase 约定或已有的 System

### 3.3 依赖倒置原则 (Dependency Inversion)

```
现在 (错误的方向):
    System → GetPrimitiveBatchPipeline() → PrimitiveBatchPipeline
    System → 知道具体的 Pipeline 类型

目标 (正确的方向):
    System → GetRenderPipeline("Primitive") → RenderPipelineBase*
    System → 仅依赖抽象接口，与具体类型无关
```

### 3.4 明确的生命周期管理

```
Pipeline 生命周期:
    创建 (SystemGroup 安装) 
    → 初始化 (Context::Initialize)
    → 每帧执行 (PrepareFrame → Collect → Cull → Sort → Build → Sync → Render)
    → 关闭 (Context::Shutdown)
    → 销毁 (SystemGroup 卸载)
```

### 3.5 ⭐ ECS 框架零特定元素操作原则 (Framework Element Agnostic)

**这是本次重构最重要的设计原则!**

#### 原则描述

ECS 框架内部（Context、System 基类、RenderGraph 等）应该**完全不知道任何具体的渲染元素**（Primitive、Text、Line、Quad、Particle 等）的存在。

#### 具体要求

**❌ 不允许的做法**:

```cpp
// Context.h - 禁止！
class ECSContext {
public:
    PrimitiveBatchPipeline* GetPrimitiveBatchPipeline();  // ❌ 特定元素 API
    TextRenderPipeline* GetTextRenderPipeline();          // ❌ 特定元素 API
    LineRenderManager* GetLineRenderManager();            // ❌ 特定元素 API
    // ...每加一个元素就加一个 getter？NO!
};
```

```cpp
// System.h - 禁止！
class System {
    virtual void OnPrimitiveRender() {}   // ❌ 特定于元素的虚函数
    virtual void OnTextRender() {}        // ❌ 特定于元素的虚函数
    virtual void OnLineRender() {}        // ❌ 特定于元素的虚函数
};
```

```cpp
// RenderGraph.cpp - 禁止！
void ECSContext::Render(float dt) {
    if (has_primitives) {
        primitive_pipeline->RunCollect();     // ❌ 具体元素逻辑
        primitive_pipeline->RunCull();
    }
    if (has_texts) {
        text_pipeline->RunCollect();          // ❌ 具体元素逻辑
    }
    if (has_lines) {
        line_manager->Prepare();              // ❌ 具体元素逻辑
    }
    // ...继续堆砌 if 语句？NO!
}
```

#### ✅ 正确的做法

```cpp
// Context.h - 框架只知道抽象
class ECSContext {
public:
    // 完全不知道 Primitive/Text/Line/Quad 的存在
    RenderPipelineBase* GetRenderPipeline(const std::string& name);  // ✅ 通用接口
    void RegisterRenderPipeline(const std::string& name,             // ✅ 通用接口
                               std::unique_ptr<RenderPipelineBase> p);
};
```

```cpp
// System.h - 框架提供通用的 System 基类
class RenderPipelineSystem : public System {
protected:
    virtual RenderPipelineBase* GetPipeline(ECSContext* ctx) = 0;
    // 子类只需实现"我的 Pipeline 名字是什么"和"调用哪个 Pipeline 方法"
};
```

```cpp
// RenderGraph.cpp - 框架通过 Pipeline 接口统一迭代
void ECSContext::Render(float dt) {
    // 框架代码与具体元素无关，适用于任何数量的元素
    for (const auto& group_name : GetSystemGroupNames()) {
        if (!IsSystemGroupEnabled(group_name)) continue;
        
        auto pipeline = GetRenderPipeline(group_name);
        if (!pipeline) continue;
        
        pipeline->PrepareFrame();
        pipeline->RunCollect();
        pipeline->RunCull();
        pipeline->RunSort();
        pipeline->RunBuild();
        pipeline->RunSync();
        // 在 BeginRenderPass 中
        pipeline->GetRenderPrimitives(...);
        pipeline->Render(cmd);
    }
    // ✅ 这段代码对任何元素数量都有效，无需改动!
}
```

#### 为什么这很重要？

| 问题 | 影响 | 解决方案 |
|------|------|---------|
| **每加一个元素就改 Context** | Context 变成怪物，而且元素越多改动越复杂 | 通过统一接口 GetRenderPipeline() |
| **System 需要了解具体元素** | System 代码重复，难以维护 | 提供 RenderPipelineSystem 基类 |
| **RenderGraph 充满不同元素的特殊逻辑** | 难以理解、易出错、难以测试 | 抽象统一的 Pipeline 执行循环 |
| **添加第 5、6、7... 个元素** | 每次都要改框架代码 | Framework 可以无限扩展，无需改动 |

#### 影响范围

```
需要梳理的代码：
1. ECSContext - 完全隔离
2. System / RenderGraph - 完全通过 RenderPipeline 接口操作
3. SystemGroup 独立管理每个 Pipeline 的生命周期（安装、卸载）
4. 没有任何 if/switch/特殊处理语句涉及具体元素
```

---

## 架构设计

### 4.1 现有基础设施

#### RenderPipelineBase (已实现 ✅)

```cpp
class RenderPipelineBase {
public:
    virtual const std::string& GetName() const = 0;
    virtual ECSContext* GetWorld() const = 0;
    
    virtual bool PrepareFrame() = 0;
    virtual void RunCollect() = 0;
    virtual void RunCull() {}
    virtual void RunSort() {}
    virtual void RunBuild() = 0;
    virtual void RunSync() {}
    virtual void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>&) const = 0;
    virtual void Render(hgl::graph::RenderCmdBuffer* cmd) = 0;
};
```

#### Context 中的 Pipeline 管理 (已实现 ✅)

```cpp
class ECSContext {
private:
    std::unordered_map<std::string, std::unique_ptr<RenderPipelineBase>> render_pipelines;
    
public:
    RenderPipelineBase* GetRenderPipeline(const std::string& name);
    void RegisterRenderPipeline(const std::string& name, 
                               std::unique_ptr<RenderPipelineBase> pipeline);
    bool IsRenderPipelineEnabled(const std::string& name) const;
    std::vector<std::string> GetRenderPipelineNames() const;
};
```

### 4.2 待实现的部分

#### 4.2.1 System 调度的统一化

创建一组新的虚基类来标准化 System 的调度模式：

```cpp
// inc/hgl/ecs/core/RenderPipelineSystem.h

namespace hgl::ecs {
    /**
     * RenderPipelineSystem - 所有 Pipeline 相关 System 的基类
     * 提供统一的生命周期和错误处理
     */
    class RenderPipelineSystem : public System {
    protected:
        /// 获取关联的 Pipeline（由子类指定）
        virtual RenderPipelineBase* GetPipeline(ECSContext* context) = 0;
        
    public:
        /// 验证 Pipeline 是否有效
        bool ValidatePipeline(ECSContext* context);
    };
    
    // 具体的调度 System 基类
    
    /// Collect 阶段 System
    class CollectSystem : public RenderPipelineSystem {
    protected:
        void Update(float dt) override final;
    private:
        virtual void OnCollect(RenderPipelineBase* pipeline) = 0;
    };
    
    /// Cull 阶段 System
    class CullSystem : public RenderPipelineSystem {
        void Update(float dt) override final;
    private:
        virtual void OnCull(RenderPipelineBase* pipeline) = 0;
    };
    
    /// Sort 阶段 System
    class SortSystem : public RenderPipelineSystem {
        void Update(float dt) override final;
    private:
        virtual void OnSort(RenderPipelineBase* pipeline) = 0;
    };
    
    /// Build 阶段 System
    class BuildSystem : public RenderPipelineSystem {
        void Update(float dt) override final;
    private:
        virtual void OnBuild(RenderPipelineBase* pipeline) = 0;
    };
    
    /// Sync 阶段 System
    class SyncSystem : public RenderPipelineSystem {
        void Update(float dt) override final;
    private:
        virtual void OnSync(RenderPipelineBase* pipeline) = 0;
    };
    
    /// Render 阶段 System
    class RenderPipelineDrawSystem : public RenderPipelineSystem {
        void Render(RenderCmdBuffer* cmd, float dt) override final;
    private:
        virtual void OnRender(RenderPipelineBase* pipeline, RenderCmdBuffer* cmd) = 0;
    };
}
```

这样做的好处：
- System 的编写变得简单：只需实现 `OnCollect()`, `OnCull()`, 等虚函数
- 自动处理错误情况：如 Pipeline 未注册、已禁用等
- 减少重复代码：每个 System 不需要再做 GetRenderPipeline() 的空值检查

#### 4.2.2 RenderPipelineGroup - Pipeline 与其 System 的统一封装（⭐ 核心架构）

**目的**：将每个 RenderPipeline 及其 3-4 个 System 打包为一个内聚的"组"，作为一个可部署的单元。

```cpp
// inc/hgl/ecs/support/RenderPipelineGroup.h
#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/ecs/core/RenderPipelineSystem.h>
#include <memory>
#include <vector>

namespace hgl::ecs {

class ECSContext;

/**
 * RenderPipelineGroup - Pipeline 与其关联 System 的统一容器
 * 
 * 职责：
 * 1. 拥有并管理一个 RenderPipelineBase 派生类实例
 * 2. 拥有并管理 1-4 个 RenderPipelineSystem 派生类实例（Collect/Cull/Build/Render）
 * 3. 统一的生命周期管理：Initialize() → Render() → Shutdown()
 * 4. 处理 System 与 Pipeline 之间的绑定
 * 
 * 示例：
 *   LineRenderPipelineGroup 包含：
 *   - LineRenderPipeline （持有 LineRenderManager）
 *   - LineCollectSystem （调用 line_pipeline->RunCollect()）
 *   - LineBatchSystem （调用 line_pipeline->RunBuild()）
 *   - LineRenderSystem （调用 line_pipeline->Render()）
 */
class RenderPipelineGroup {
protected:
    std::string name_;  // "Primitive", "Text", "Line", "Quad"等
    std::unique_ptr<RenderPipelineBase> pipeline_;
    std::vector<std::unique_ptr<RenderPipelineSystem>> systems_;
    bool enabled_ = true;

public:
    explicit RenderPipelineGroup(const std::string& name) : name_(name) {}
    virtual ~RenderPipelineGroup() = default;

    // ===== 生命周期管理 =====
    
    /// 初始化：创建 Pipeline 和 System，注册到 Context
    virtual bool Initialize(ECSContext* context) = 0;
    
    /// 关闭：清理 Pipeline 和 System
    virtual void Shutdown(ECSContext* context) = 0;

    // ===== 属性访问 =====
    
    const std::string& GetName() const { return name_; }
    RenderPipelineBase* GetPipeline() const { return pipeline_.get(); }
    const std::vector<std::unique_ptr<RenderPipelineSystem>>& GetSystems() const { return systems_; }
    
    bool IsEnabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

protected:
    /// 子类实现：创建 Pipeline 实例
    virtual std::unique_ptr<RenderPipelineBase> CreatePipeline() = 0;
    
    /// 子类实现：创建并注册 System 实例
    virtual void RegisterSystems() = 0;
};

}  // namespace hgl::ecs
```

**key design point**：
- 每个 Group 管理一个 Pipeline + 一组 System
- In `Initialize()`，group 创建 pipeline + systems，然后将 systems 注册到 Context
- In `Shutdown()`，group 负责清理 pipeline + systems
- ECS Context 持有 Groups map，而不是直接持有 Pipeline map
- Group 是**部署单元**：启用/禁用 group = 启用/禁用整个渲染元素

#### 4.2.3 每个 Pipeline 的设计

以 PrimitiveBatchPipeline 为例：

```cpp
// inc/hgl/ecs/support/PrimitiveBatchPipeline.h
class PrimitiveBatchPipeline : public RenderPipelineBase {
private:
    ECSContext* world = nullptr;
    const hgl::graph::CameraInfo* camera_info = nullptr;
    hgl::graph::VulkanDevice* device = nullptr;
    
    // 帧数据
    uint32_t prepared_frame_index = UINT32_MAX;
    std::vector<PrimitiveRenderItem*> collected_items;
    hgl::UnorderedMap<MaterialPipelineKey, MaterialBatch> material_batches;
    
public:
    const std::string& GetName() const override { return "Primitive"; }
    ECSContext* GetWorld() const override { return world; }
    void SetWorld(ECSContext* w) { world = w; }
    void SetDevice(hgl::graph::VulkanDevice* d) { device = d; }
    void SetCameraInfo(const hgl::graph::CameraInfo* c) { camera_info = c; }
    
    bool PrepareFrame() override;  // 初始化各阶段状态
    void RunCollect() override;    // 遍历 PrimitiveComponent，收集渲染项
    void RunCull() override;       // 视锥体剔除
    void RunSort() override;       // 按距离排序
    void RunBuild() override;      // 构建 MaterialBatch，写 VAB
    void RunSync() override;       // 同步描述符、UBO
    void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>&) const override;
    void Render(hgl::graph::RenderCmdBuffer* cmd) override;  // 录制绘制命令
};
```

---

## 代码组织结构

### 4.3 目录组织

为了避免代码散落四处，每个 RenderPipeline 应该有独立的目录结构。建议如下：

```
inc/hgl/ecs/
├── support/
│   ├── RenderPipelineBase.h          （已存在）
│   ├── RenderPipelineGroup.h          （新建：Group 基类）
│   ├── RenderPipelineSystem.h         （新建：System 基类）
│   │
│   ├── primitive/                     （新目录）
│   │   ├── PrimitiveBatchPipeline.h
│   │   └── PrimitiveRenderPipelineGroup.h
│   │
│   ├── text/                          （新目录）
│   │   ├── TextRenderPipeline.h
│   │   └── TextRenderPipelineGroup.h
│   │
│   ├── line/                          （新目录：从分散中收集）
│   │   ├── LineRenderPipeline.h       （新建：从 LineRenderManager 抽离）
│   │   └── LineRenderPipelineGroup.h
│   │
│   └── quad/                          （新目录：从分散中收集）
│       ├── QuadRenderPipeline.h       （新建）
│       └── QuadRenderPipelineGroup.h
│
src/ecs/
├── support/
│   ├── RenderPipelineGroup.cpp        （新建）
│   ├── RenderPipelineSystem.cpp       （新建）
│   │
│   ├── primitive/                     （新目录）
│   │   ├── PrimitiveBatchPipeline.cpp
│   │   └── PrimitiveRenderPipelineGroup.cpp
│   │
│   ├── text/                          （新目录）
│   │   ├── TextRenderPipeline.cpp
│   │   └── TextRenderPipelineGroup.cpp
│   │
│   ├── line/                          （新目录）
│   │   ├── LineRenderPipeline.cpp     （新建）
│   │   ├── LineCollectSystem.cpp
│   │   ├── LineBatchSystem.cpp
│   │   ├── LineRenderSystem.cpp       （已存在，改造）
│   │   └── LineRenderPipelineGroup.cpp
│   │
│   └── quad/                          （新目录）
│       ├── QuadRenderPipeline.cpp     （新建）
│       ├── QuadCollectSystem.cpp      （新建）
│       ├── QuadBatchSystem.cpp        （新建）
│       ├── QuadRenderSystem.cpp       （已存在，改造）
│       └── QuadRenderPipelineGroup.cpp

systems/
├── render/
│   ├── primitive/                     （可选：System 也可放在这里）
│   │   ├── RenderPrimitiveCullSystem.h
│   │   ├── RenderPrimitiveSortSystem.h
│   │   └── ...
│   │
│   ├── text/
│   │   ├── TextCollectSystem.h
│   │   └── ...
│   │
│   ├── line/
│   │   ├── LineCollectSystem.h        （新建）
│   │   ├── LineBatchSystem.h          （新建）
│   │   └── LineRenderSystem.h         （改造）
│   │
│   └── quad/
│       ├── QuadCollectSystem.h        （新建）
│       ├── QuadBatchSystem.h          （新建）
│       └── QuadRenderSystem.h         （改造）
```

**关键原则**：
- ✅ **每个 Pipeline 类型独占一个子目录** （primitive/text/line/quad）
- ✅ **目录包含 Pipeline、Group、System 的完整实现**
- ✅ **对应的 .h 和 .cpp 都在同一目录**
- ✅ **便于管理和未来的按需编译**
- ✅ **新增 Pipeline 时只需创建新目录，无需修改其他目录**

#### 旧代码迁移（Line/Quad）

当前的 Line 和 Quad 实现分散在：
- `inc/hgl/render/LineRenderManager.h`
- `src/SceneGraph/render/line/` （多个文件）
- `inc/hgl/ecs/systems/render/LineRenderSystem.h`
- `src/ecs/systems/render/LineRenderSystem.cpp`

**迁移策略**：
1. 在 `inc/hgl/ecs/support/line/` 创建新头文件：`LineRenderPipeline.h`
2. 在 `src/ecs/support/line/` 创建新实现：`LineRenderPipeline.cpp`, `LineRenderPipelineGroup.cpp`
3. `LineRenderManager` 保留原位置（它是纯数据/算法类）
4. 将 `LineRenderManager` 的所有权转移给 `LineRenderPipeline`（而非 LineRenderSystem）
5. 逐步关闭旧 System 中的业务逻辑，改为委托调用

---

## 具体迁移步骤

### 5.1 迁移顺序和依赖（使用 RenderPipelineGroup 架构）

```
┌──────────────────────────────────────────────────────────┐
│ Phase 1: 基础设施 (已完成)                                 │
│ - RenderPipelineBase                                     │
│ - RenderPipelineGroup（新的统一容器）                     │
│ - RenderPipelineSystem 虚基类                            │
│ - Context::RegisterRenderPipeline()                      │
│ - Context::GetRenderPipeline()                           │
└──────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────┐
│ Phase 2a: Primitive 迁移 (推荐首先 ✅ 风险最低)            │
│ - 创建 PrimitiveRenderPipelineGroup                      │
│   ├─ PrimitiveBatchPipeline (继承 RenderPipelineBase)   │
│   ├─ RenderPrimitiveCullSystem (使用新的 CallectSystem)   │
│   ├─ RenderPrimitiveSortSystem (使用新的基类)             │
│   ├─ RenderPrimitiveBatchBuildSystem (使用新的 BuildSystem) │
│   └─ RenderPrimitiveBatchFinalizeSystem                  │
│ - Group::Initialize() 将 Pipeline 注册到 Context        │
│ - 测试完整性                                             │
└──────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────┐
│ Phase 2b: Text 迁移 (同步进行)                           │
│ - 创建 TextRenderPipelineGroup                          │
│   ├─ TextRenderPipeline (继承 RenderPipelineBase)       │
│   ├─ TextCollectSystem (使用新的基类)                     │
│   ├─ TextResourceSyncSystem (改造)                      │
│   └─ TextBuildSystem (改造)                             │
│ - Group::Initialize() 将 Pipeline 注册到 Context        │
│ - 测试完整性                                             │
└──────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────┐
│ Phase 2c: Line 迁移 (⭐ 关键迁移，从混合体分离)            │
│ - 创建 LineRenderPipelineGroup                          │
│   ├─ LineRenderPipeline (新建，从 LineRenderManager 抽离) │
│   ├─ LineCollectSystem (新建，薄代理)                    │
│   ├─ LineBatchSystem (新建，薄代理)                      │
│   └─ LineRenderSystem (改造，改为薄代理)                  │
│ - Group::Initialize():                                   │
│   ├─ 创建 LineRenderPipeline，拥有 LineRenderManager    │
│   ├─ 创建 3 个 System，注册到 Context                    │
│   ├─ 注册 Pipeline 到 Context::render_pipelines         │
│   └─ 返回 true                                          │
│ - Group::Shutdown() 清理所有资源                         │
│ - 目录组织：inc/hgl/ecs/support/line/{.h}               │
│ - 删除 LineRenderManager 的分散引用                       │
│ - 测试完整性                                             │
└──────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────┐
│ Phase 2d: Quad 迁移 (⭐ 关键迁移，从混合体分离)            │
│ - 创建 QuadRenderPipelineGroup (与 Phase 2c 完全相同的模式) │
│   ├─ QuadRenderPipeline (新建，与 QuadRenderManager 解耦)  │
│   ├─ QuadCollectSystem (新建，薄代理)                    │
│   ├─ QuadBatchSystem (新建，薄代理)                      │
│   └─ QuadRenderSystem (改造，改为薄代理)                  │
│ - 目录组织：inc/hgl/ecs/support/quad/{.h}               │
│ - 删除早前的 QuadResourcePrepareSystem 中混合的逻辑        │
│ - 测试完整性                                             │
└──────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────┐
│ Phase 3: ⭐ ECS Context 纯净化 (本阶段核心目标)            │
│ ─────────────────────────────────────────────────────────│
│ 当前 Context 持有：                                       │
│   ✗ PrimitiveBatchPipeline* primitive_batch_pipeline;   │
│   ✗ TextRenderPipeline* text_render_pipeline;           │
│   ✗ … 每加一种元素就加一个 getter                        │
│                                                         │
│ 目标 Context 应该持有：                                   │
│   ✓ std::unordered_map<std::string,                     │
│       std::unique_ptr<RenderPipelineBase>>               │
│   ✓ 统一的 GetRenderPipeline(name) API                  │
│   ✓ 完全不知道 Primitive/Text/Line/Quad 具体实现        │
│                                                         │
│ 具体步骤：                                               │
│ 3.1 删除 Context 中的特定元素 getter                     │
│     - 删除 GetPrimitiveBatchPipeline()                  │
│     - 删除 GetTextRenderPipeline()                      │
│ 3.2 确保所有调用处通过 GetRenderPipeline(name) 访问      │
│ 3.3 验证：Context 代码中不存在 "Primitive" "Text" 等词    │
│ 3.4 全局编译和单元测试                                  │
└──────────────────────────────────────────────────────────┘
                         ↓
┌──────────────────────────────────────────────────────────┐
│ Phase 4: 文档和示例                                       │
│ - 更新架构文档                                            │
│ - 创建"如何添加新的 Pipeline"指南                         │
│ - 验收标准检查清单                                        │
└──────────────────────────────────────────────────────────┘
```
│ Phase 4: 文档和示例                             │
│ - 更新开发文档                                  │
│ - 新增 Pipeline 实现示例                        │
│ - 性能基准对比                                  │
└─────────────────────────────────────────────────┘
```

### 5.2 Phase 2a 详细步骤 - Primitive 迁移

#### Step 1: 修改 PrimitiveBatchPipeline.h

```diff
- class PrimitiveBatchPipeline {
+ class PrimitiveBatchPipeline : public RenderPipelineBase {
  private:
      ECSContext* world = nullptr;
      ...
  
  public:
+     const std::string& GetName() const override { return "Primitive"; }
+     ECSContext* GetWorld() const override { return world; }
      void SetWorld(ECSContext* w) { world = w; }
      
+     bool PrepareFrame() override;
-     bool PrepareFrame(ECSContext* ctx);
      
      void RunCulling();
+     void RunCull() override { RunCulling(); }
      
      void RunSorting();
+     void RunSort() override { RunSorting(); }
      
      void RunTransformIndexing();
      void RunBatching();
+     void RunBuild() override { 
+         RunTransformIndexing();
+         RunBatching();
+     }
```

#### Step 2: 修改 RenderPrimitiveCullSystem.h

```diff
- class RenderPrimitiveCullSystem : public System {
+ class RenderPrimitiveCullSystem : public CullSystem {
  private:
-     void Update(float deltaTime) override;
+     void OnCull(RenderPipelineBase* pipeline) override {
+         if (auto p = dynamic_cast<PrimitiveBatchPipeline*>(pipeline))
+             p->RunCulling();
+     }
  };
```

#### Step 3: 更新 System 调度顺序

在 `inc/hgl/ecs/core/DefaultSystems.h` 中确保 System 的 ExecutionPhase 正确：

```cpp
RenderPrimitiveCullSystem::ExecutionPhase = RenderCull;
RenderPrimitiveSortSystem::ExecutionPhase = RenderSort;
RenderPrimitiveBatchBuildSystem::ExecutionPhase = RenderBatch;
RenderPrimitiveBatchFinalizeSystem::ExecutionPhase = RenderBatch;  // 与 Build 同阶段但有顺序
```

#### Step 4: 编译和测试

```bash
cmake --build build --config Debug --target ULRE.ECS
# 运行 Primitive 相关单元测试
# 运行 GizmoUsageExample 确保正常工作
```

### 5.3 Phase 2b 详细步骤 - Text 迁移

类似 Phase 2a，TextRenderPipeline 改为继承 RenderPipelineBase，调整三个 System。

### 5.4 Phase 2c 详细步骤 - Line 迁移

#### Step 1: 分离 LineRenderPipeline

**当前问题**：LineRenderSystem 是混合体 System，同时包含：
- System 职责（被 ECS 调度）
- Pipeline 职责（数据管理、渲染逻辑）
- Manager 职责（LineRenderManager 成员）

**目标**：分离成 4 个独立组件：

| 组件 | 职责 | 继承类 | 文件位置 |
|-----|------|-------|--------|
| **LineRenderPipeline** | 数据管理、渲染逻辑 | RenderPipelineBase | inc/hgl/ecs/support/ |
| **LineCollectSystem** | Collect 阶段委托 | CollectSystem | inc/hgl/ecs/systems/render/ |
| **LineBatchSystem** | Build 阶段委托 | BuildSystem | inc/hgl/ecs/systems/render/ |
| **LineRenderSystem** | Draw 阶段委托 | RenderPipelineDrawSystem | inc/hgl/ecs/systems/render/ |

**1.1 创建 LineRenderPipeline**

```cpp
// inc/hgl/ecs/support/LineRenderPipeline.h
#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <hgl/render/LineRenderManager.h>

class LineRenderPipeline : public RenderPipelineBase {
private:
    std::unique_ptr<LineRenderManager> line_manager;
    bool prepared = false;

public:
    LineRenderPipeline();
    ~LineRenderPipeline() = default;

    // 从 RenderPipelineBase 实现的虚方法
    bool PrepareFrame() override;
    void RunCollect() override;
    void RunCull() override;
    void RunSort() override;
    void RunBuild() override;
    void RunSync() override;
    void GetRenderPrimitives(RenderPrimitives& out) override;
    void Render(RenderCmdBuffer* cmd) override;

    // Pipeline 特定接口（供 System 使用）
    LineRenderManager* GetManager() const { return line_manager.get(); }

    void Shutdown() override;
};
```

**1.2 实现 LineRenderPipeline**

```cpp
// src/ecs/support/LineRenderPipeline.cpp
#include <hgl/ecs/support/LineRenderPipeline.h>

LineRenderPipeline::LineRenderPipeline() {
    line_manager = std::make_unique<LineRenderManager>();
}

bool LineRenderPipeline::PrepareFrame() {
    if (!prepared) {
        line_manager->PrepareFrame();
        prepared = true;
    }
    return true;
}

void LineRenderPipeline::RunCollect() {
    line_manager->CollectLines();
}

void LineRenderPipeline::RunCull() {
    line_manager->CullLines();
}

void LineRenderPipeline::RunSort() {
    line_manager->SortLines();
}

void LineRenderPipeline::RunBuild() {
    line_manager->BuildBatches();
}

void LineRenderPipeline::RunSync() {
    line_manager->SyncGPU();
}

void LineRenderPipeline::GetRenderPrimitives(RenderPrimitives& out) {
    line_manager->GetPrimitives(out);
}

void LineRenderPipeline::Render(RenderCmdBuffer* cmd) {
    line_manager->RecordDrawCalls(cmd);
}

void LineRenderPipeline::Shutdown() {
    line_manager.reset();
}
```

**1.3 创建 LineCollectSystem（Collect 阶段委托）**

```cpp
// inc/hgl/ecs/systems/render/LineCollectSystem.h
#pragma once

#include <hgl/ecs/systems/support/RenderPipelineSystem.h>

class LineCollectSystem : public CollectSystem {
public:
    explicit LineCollectSystem(ECSContext* ctx) : CollectSystem(ctx) {}

    std::string GetName() const override { return "LineCollectSystem"; }

    RenderPipelineBase* GetPipeline(ECSContext* ctx) override {
        return ctx->GetRenderPipeline("Line");
    }

    void OnCollect(RenderPipelineBase* pipeline) override {
        // 这就是委托的全部：调用 Pipeline 的 RunCollect
        if (pipeline) {
            pipeline->RunCollect();
        }
    }
};
```

**1.4 创建 LineBatchSystem（Build 阶段委托）**

```cpp
// inc/hgl/ecs/systems/render/LineBatchSystem.h
#pragma once

#include <hgl/ecs/systems/support/RenderPipelineSystem.h>

class LineBatchSystem : public BuildSystem {
public:
    explicit LineBatchSystem(ECSContext* ctx) : BuildSystem(ctx) {}

    std::string GetName() const override { return "LineBatchSystem"; }

    RenderPipelineBase* GetPipeline(ECSContext* ctx) override {
        return ctx->GetRenderPipeline("Line");
    }

    void OnBuild(RenderPipelineBase* pipeline) override {
        if (pipeline) {
            pipeline->RunCull();
            pipeline->RunSort();
            pipeline->RunBuild();
        }
    }
};
```

**1.5 改造 LineRenderSystem（Draw 阶段委托）**

```cpp
// inc/hgl/ecs/systems/render/LineRenderSystem.h（改造）
#pragma once

#include <hgl/ecs/systems/support/RenderPipelineSystem.h>

class LineRenderSystem : public RenderPipelineDrawSystem {
public:
    explicit LineRenderSystem(ECSContext* ctx) : RenderPipelineDrawSystem(ctx) {}

    std::string GetName() const override { return "LineRenderSystem"; }

    RenderPipelineBase* GetPipeline(ECSContext* ctx) override {
        return ctx->GetRenderPipeline("Line");
    }

    // 注意：父类的 OnRender 方法会调用 pipeline->Render(cmd_buffer)
    // LineRenderSystem 现在无需覆盖，只需提供 GetPipeline
};
```

**1.6 更新 DefaultSystems.h**

在初始化系统时，按顺序注册三个 Line 系统：

```cpp
// inc/hgl/ecs/core/DefaultSystems.h
// 添加系统到正确的阶段：

auto line_collect = std::make_unique<LineCollectSystem>(context);
line_collect->SetExecutionPhase(ECSContext::RenderCollect);
context->AddSystem(std::move(line_collect));

auto line_batch = std::make_unique<LineBatchSystem>(context);
line_batch->SetExecutionPhase(ECSContext::RenderBatch);
context->AddSystem(std::move(line_batch));

auto line_render = std::make_unique<LineRenderSystem>(context);
line_render->SetExecutionPhase(ECSContext::RenderDraw);
context->AddSystem(std::move(line_render));
```

**1.7 在 Context 初始化中注册 LineRenderPipeline**

```cpp
// src/ecs/core/Context.cpp (ECSContext::Initialize) 中添加：

auto line_pipeline = std::make_unique<LineRenderPipeline>();
RegisterRenderPipeline("Line", std::move(line_pipeline));
```

#### Step 2: 编译和初步检查

在项目中构建新的 Pipeline 和 System：

```bash
cd e:\ULRE
cmake --build build --config Debug --target ULRE.ECS
```

**预期结果**：
- ✅ 编译通过（可能有一些定义缺失的链接错误，这是正常的）
- ✅ 新的 System 文件被包含在编译中

如果出现编译错误：
- 检查 `#include` 路径是否正确
- 确保 `CollectSystem`, `BuildSystem`, `RenderPipelineDrawSystem` 基类存在
- 检查 `LineRenderManager` 是否可从 `<hgl/render/LineRenderManager.h>` 导入

#### Step 3: 迁移 LineRenderManager 内部逻辑

**当前状态**：LineRenderManager 可能存在于 LineRenderSystem 中或作为单独的类。

**操作**：
1. 检查 `inc/hgl/render/LineRenderManager.h` 中 LineRenderManager 的完整接口
2. 确保所有必要的方法都在 LineRenderPipeline::RunXxx() 中被正确委托
3. LineRenderManager 应该保持**私有**，不暴露给 System

**检查清单**：
- [ ] LineRenderManager::CollectLines() 在 RunCollect() 中被调用
- [ ] LineRenderManager::CullLines() 在 RunCull() 中被调用
- [ ] LineRenderManager::SortLines() 在 RunSort() 中被调用
- [ ] LineRenderManager::BuildBatches() 在 RunBuild() 中被调用
- [ ] LineRenderManager::SyncGPU() 在 RunSync() 中被调用
- [ ] LineRenderManager::RecordDrawCalls(cmd) 在 Render(cmd) 中被调用

#### Step 4: 删除旧的 LineRenderSystem（混合体版本）

**警告**：此步骤会改变现有的 LineRenderSystem 行为。需要谨慎进行。

在 `inc/hgl/ecs/systems/render/LineRenderSystem.h` 中：

```cpp
// 删除这些成员和方法：
// - std::unique_ptr<LineRenderManager> manager;
// - void Update() override { ... }  // 旧的业务逻辑
// - void Render(RenderCmdBuffer*) override { ... }

// 只保留新的瘦代理实现（见 Step 1.5）
```

#### Step 5: 修复任何调用点

搜索代码库中直接使用 LineRenderSystem 的地方：

```bash
# 在 VS Code 中使用 Ctrl+Shift+F 搜索：
# "LineRenderSystem"  
# 和 ".line_render_system" 或 "context->line_render_system"
# 和 "line_manager->"
```

**需要修改**：
- 任何直接访问 `line_manager` 的代码应改为访问 Pipeline
- 示例修改模板：
  ```cpp
  // 之前
  context->GetLineRenderSystem()->GetManager()->AddLine(...);
  
  // 之后（假设 rendering 代码能访问 Pipeline）
  auto line_pipeline = context->GetRenderPipeline("Line");
  auto* line_mgr = dynamic_cast<LineRenderPipeline*>(line_pipeline)->GetManager();
  line_mgr->AddLine(...);
  ```

#### Step 6: 运行 Line 相关的单元测试

假设项目中有针对 Line 渲染的单元测试：

```bash
# 构建并运行测试
cmake --build build --config Debug --target LineRenderTests
# 或运行整个测试套件
ctest --output-on-failure
```

**需要通过**：
- ✅ 简单 Line 添加和绘制
- ✅ Line 批处理逻辑（多条线合并）
- ✅ Line 裁剪（Cull）功能
- ✅ Line 排序（Sort）功能
- ✅ Line 颜色和样式变化

#### Step 7: 综合测试 - 运行渲染示例

```bash
# 构建完整项目
cmake --build build --config Debug --target ULRE

# 运行包含 Line 渲染的示例应用
# 例如：EditorApp, GizmoUsageExample 等
```

**验证清单**：
- [ ] 应用启动时无崩溃
- [ ] Line 正常渲染（在 3D 视图中可见）
- [ ] 多条 Line 正确批处理（性能未下降）
- [ ] Line 颜色、宽度等属性生效
- [ ] 与其他渲染元素（Primitive、Text）共存无冲突

#### Step 8: 性能验证

检查 Line 渲染性能是否与迁移前一致：

```cpp
// 在 RenderGraph::Execute 中添加简单的计时（临时）
auto line_pipeline = context->GetRenderPipeline("Line");
if (line_pipeline) {
    auto start = std::chrono::high_resolution_clock::now();
    line_pipeline->RunCollect();
    line_pipeline->RunCull();
    line_pipeline->RunSort();
    line_pipeline->RunBuild();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();
    HLOGD("[Line] Pipeline execution: {:.3f} ms", duration_ms);
}
```

**目标**：Line 单独执行时间应 < 1 ms（对于中等复杂度场景）

### 5.5 Phase 2d 详细步骤 - Quad 迁移

**前提条件**：与 Phase 2c（Line 迁移）的流程完全相同，只是将 `Line` 替换为 `Quad`。

#### Step 1: 分离 QuadRenderPipeline 和创建三个 System

与 Phase 2c Step 1 完全相同的步骤：

1. 创建 `QuadRenderPipeline` 继承 `RenderPipelineBase`
2. 创建 `QuadCollectSystem : public CollectSystem`
3. 创建 `QuadBatchSystem : public BuildSystem`
4. 改造 `QuadRenderSystem : public RenderPipelineDrawSystem`（如果存在混合体版本）
5. 在 Context::Initialize 中注册：`RegisterRenderPipeline("Quad", quad_pipeline)`
6. 在 DefaultSystems.h 中按顺序添加三个 System

**示例代码模板**（所有类都遵循与 Line 完全相同的模式）：

```cpp
// 在三个 System 的 GetPipeline() 中返回：
RenderPipelineBase* GetPipeline(ECSContext* ctx) override {
    return ctx->GetRenderPipeline("Quad");  // 只改这一行
}

// 在 OnCollect / OnBuild / OnRender 中委托给 Pipeline
void OnCollect(RenderPipelineBase* pipeline) override {
    if (pipeline) pipeline->RunCollect();
}
```

#### Step 2-9: 编译、测试、验证

完全参照 Phase 2c 的 Step 2-8 执行（编译、单元测试、集成测试、性能验证）。

**关键验证**：
- ✅ Quad 创建、绘制、批处理正常
- ✅ 与 Line、Primitive、Text 共存无冲突
- ✅ 性能无下降

#### 总结：Phase 2d 努力量

预估时间：**0.5-1 天**（几乎与 Phase 2c 的模板完全相同）

复杂度：**极低**（全部代码模板化，可自动生成）

---

### 5.6 Phase 3 - ⭐ ECS Context 纯净化（本阶段核心目标）

**这是整个重构的最关键步骤！** 目的是彻底清理 Context，实现"ECS 框架零特定元素知识"的设计原则。

#### 为什么这很重要？

**当前问题**：
```cpp
class ECSContext {
    // Context 充斥着特定元素的代码
    PrimitiveBatchPipeline* primitive_batch_pipeline;     // ← Primitive 特定
    TextRenderPipeline* text_render_pipeline;             // ← Text 特定
    // 还要加 Line? Quad? Particle?
};
```

**后果**：
- ❌ 每增加一种渲染元素，Context 就要修改和重新编译
- ❌ Context 知道了所有具体的 Pipeline 类型，打破了依赖倒置
- ❌ 代码不能优雅地扩展，违背开闭原则
- ❌ Context 变成了"框架怪物"，职责不单一

**目标状态**：
```cpp
class ECSContext {
    // Context 完全抽象，不知道任何具体元素
    std::unordered_map<std::string, std::unique_ptr<RenderPipelineBase>> render_pipelines;
    // ↑ 这就够了！任意数量的 Pipeline，无需改 Context!
};
```

**当添加第 10 个渲染元素时**：
- ✅ 只需创建新的 Pipeline + Group
- ✅ Context 代码**零修改**
- ✅ 框架自动支持（通过统一的 GetRenderPipeline() 接口）

#### 验收标准

Phase 3 完成后，Context.h/.cpp 中**不应该出现**以下词汇：
- ❌ `Primitive`（除了在注释或日志中）
- ❌ `Text`（除了在注释或日志中）
- ❌ `Line`（除了在注释或日志中）
- ❌ `Quad`（除了在注释或日志中）
- ❌ `GetPrimitiveBatchPipeline()`
- ❌ `GetTextRenderPipeline()`
- ❌ 任何针对特定元素的 if-switch 语句

#### Step 1: 删除特定元素的 getter

在 `inc/hgl/ecs/core/Context.h` 中，删除：

```cpp
// ❌ 删除这些—它们是"特定元素耦合"的罪魁祸首
PrimitiveBatchPipeline* GetPrimitiveBatchPipeline();    // REMOVE
TextRenderPipeline* GetTextRenderPipeline();            // REMOVE
LineRenderPipeline* GetLineRenderPipeline();            // （如果存在）REMOVE
QuadRenderPipeline* GetQuadRenderPipeline();            // （如果存在）REMOVE

// ✅ 只保留通用 API
RenderPipelineBase* GetRenderPipeline(const std::string& name);
```

#### Step 2: 更新所有调用处

使用全局搜索（Ctrl+Shift+F）查找所有调用：
- `GetPrimitiveBatchPipeline()`
- `GetTextRenderPipeline()`  
- `GetLineRenderPipeline()`
- `GetQuadRenderPipeline()`

对于每个调用，替换模板：

```cpp
// 之前
context->GetPrimitiveBatchPipeline()->RunCulling();

// 之后
auto pipeline = context->GetRenderPipeline("Primitive");
if (pipeline) {
    pipeline->RunCull();  // 注: 通过虚接口调用，type-safe
}
```

**关键**：所有调用都改为通过字符串 `"Primitive"` / `"Text"` / `"Line"` / `"Quad"` 获取。

#### Step 3: 删除或迁移成员变量

在 `inc/hgl/ecs/core/Context.h` 的 **private** 部分：

```cpp
private:
    // ❌ 删除这些特定元素的成员
    std::unique_ptr<PrimitiveBatchPipeline> primitive_batch_pipeline;    // DELETE
    std::unique_ptr<TextRenderPipeline> text_render_pipeline;            // DELETE
    // ...
    
    // ✅ 只保留统一的 map
    std::unordered_map<std::string, std::unique_ptr<RenderPipelineBase>> render_pipelines;
```

#### Step 4: 更新 Context 的初始化

在 `src/ecs/core/Context.cpp` 的 `Initialize()` 方法中：

```cpp
bool ECSContext::Initialize() {
    // 之前
    // primitive_batch_pipeline = std::make_unique<PrimitiveBatchPipeline>();  // DELETE
    // text_render_pipeline = std::make_unique<TextRenderPipeline>();          // DELETE
    
    // 之后：RenderPipelineGroup 的 Initialize() 负责创建 Pipeline
    // Context 只需要这样：
    for (auto& [group_name, group] : render_pipeline_groups) {
        if (!group->Initialize(this)) {
            HLOGE("Failed to initialize pipeline group: {}", group_name);
            return false;
        }
    }
    return true;
}
```

#### Step 5: 更新 Context 的清理

在 `Shutdown()` 方法中：

```cpp
void ECSContext::Shutdown() {
    // 之前的特定清理逻辑删除
    // if (text_render_pipeline) text_render_pipeline->Shutdown();        // DELETE
    // if (primitive_batch_pipeline) primitive_batch_pipeline->Shutdown();  // DELETE
    
    // 统一清理（RenderPipelineGroup 负责）
    for (auto& [group_name, group] : render_pipeline_groups) {
        group->Shutdown(this);
    }
    render_pipeline_groups.clear();
    render_pipelines.clear();
}
```

#### Step 6: 编译全局测试

```bash
cd e:\ULRE
cmake --build build --config Debug

# 应该看到：
# - Context.h/.cpp 中没有特定元素的引用
# - 所有引用都通过 GetRenderPipeline(name) 完成
# - 编译成功（EXIT=0）
```

**验证**：在编译输出中搜索 "undefined reference to GetPrimitiveBatchPipeline"
- 如果出现，说明还有遗漏的调用点需要更新
- 应该**零条警告**

#### Step 7: 验收清单

完成 Phase 3 后，检查：

- [ ] Context.h 中没有 PrimitiveBatchPipeline 成员
- [ ] Context.h 中没有 TextRenderPipeline 成员  
- [ ] Context.h 中没有 GetPrimitiveBatchPipeline() 方法
- [ ] Context.h 中没有 GetTextRenderPipeline() 方法
- [ ] Context.cpp 中没有创建特定 Pipeline 的代码
- [ ] 所有 Pipeline 访问都通过 GetRenderPipeline(name)
- [ ] Context 代码中不出现 "Primitive", "Text", "Line", "Quad" 等词（除注释外）
- [ ] 全局编译通过（EXIT=0）
- [ ] 单元测试通过
- [ ] 示例应用（EditorApp, GizmoUsageExample）运行正常

---

## 代码示例

### 6.1 如何实现一个新的 Pipeline

比如实现 ParticleRenderPipeline（粒子渲染）：

```cpp
// inc/hgl/ecs/support/ParticleRenderPipeline.h
#pragma once

#include <hgl/ecs/support/RenderPipelineBase.h>
#include <vector>
#include <memory>

namespace hgl::ecs {
    class ParticleComponent;
    
    class ParticleRenderPipeline : public RenderPipelineBase {
    private:
        ECSContext* world = nullptr;
        hgl::graph::VulkanDevice* device = nullptr;
        std::vector<const ParticleComponent*> active_particles;
        std::vector<hgl::graph::Primitive*> render_primitives;
        
    public:
        ParticleRenderPipeline() = default;
        ~ParticleRenderPipeline() override = default;
        
        const std::string& GetName() const override { return "Particle"; }
        ECSContext* GetWorld() const override { return world; }
        
        void SetWorld(ECSContext* w) { world = w; }
        void SetDevice(hgl::graph::VulkanDevice* d) { device = d; }
        
        bool PrepareFrame() override;
        void RunCollect() override;
        void RunBuild() override;
        void GetRenderPrimitives(std::vector<hgl::graph::Primitive*>&) const override;
        void Render(hgl::graph::RenderCmdBuffer* cmd) override;
    };
}
```

```cpp
// src/ecs/support/ParticleRenderPipeline.cpp
namespace hgl::ecs {
    bool ParticleRenderPipeline::PrepareFrame() {
        // 初始化每帧状态
        active_particles.clear();
        render_primitives.clear();
        return true;
    }
    
    void ParticleRenderPipeline::RunCollect() {
        if (!world || !device) return;
        
        // 遍历世界中的所有 ParticleComponent
        std::vector<std::shared_ptr<ParticleComponent>> particles;
        world->GetComponents(particles);
        
        for (auto& particle : particles) {
            if (particle && particle->IsVisible())
                active_particles.push_back(particle.get());
        }
    }
    
    void ParticleRenderPipeline::RunBuild() {
        // 构建粒子批次、更新 VAB 等
        for (auto particle : active_particles) {
            // 更新粒子位置、颜色等
            // 标记 GPU 缓冲为脏（自动上传）
        }
    }
    
    void ParticleRenderPipeline::GetRenderPrimitives(
        std::vector<hgl::graph::Primitive*>& out) const {
        out.insert(out.end(), render_primitives.begin(), render_primitives.end());
    }
    
    void ParticleRenderPipeline::Render(hgl::graph::RenderCmdBuffer* cmd) {
        if (!cmd || render_primitives.empty()) return;
        
        // 录制粒子绘制命令
        for (auto prim : render_primitives) {
            cmd->DrawPrimitive(prim);
        }
    }
}
```

然后所需的 System 就很简单了：

```cpp
// inc/hgl/ecs/systems/render/ParticleCollectSystem.h
class ParticleCollectSystem : public CollectSystem {
private:
    RenderPipelineBase* GetPipeline(ECSContext* context) override {
        return context->GetRenderPipeline("Particle");
    }
    
    void OnCollect(RenderPipelineBase* pipeline) override {
        if (auto p = dynamic_cast<ParticleRenderPipeline*>(pipeline))
            p->RunCollect();
    }
};

// inc/hgl/ecs/systems/render/ParticleRenderSystem.h
class ParticleRenderSystem : public RenderPipelineDrawSystem {
private:
    RenderPipelineBase* GetPipeline(ECSContext* context) override {
        return context->GetRenderPipeline("Particle");
    }
    
    void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override {
        if (auto p = dynamic_cast<ParticleRenderPipeline*>(pipeline))
            p->Render(cmd);
    }
};
```

### 6.2 安装 SystemGroup 的方式

```cpp
// src/ecs/core/DefaultSystems.cpp

bool InstallParticleGroup(ECSContext* context, hgl::graph::IRenderTarget* target) {
    // 1. 创建并注册 Pipeline
    auto particle_pipeline = std::make_unique<ParticleRenderPipeline>();
    particle_pipeline->SetWorld(context);
    particle_pipeline->SetDevice(context->GetGPUDevice());
    context->RegisterRenderPipeline("Particle", std::move(particle_pipeline));
    
    // 2. 注册 System
    if (!context->RegisterRenderSystem<ParticleCollectSystem>())
        return false;
    
    if (!context->RegisterRenderSystem<ParticleBatchSystem>())
        return false;
        
    if (!context->RegisterRenderSystem<ParticleRenderSystem>())
        return false;
    
    // 3. 设置各 System 的执行顺序（由 ExecutionPhase 自动管理）
    
    return true;
}

// 在 SystemGroupRegistry 中注册
void RegisterDefaultSystemGroups() {
    SystemGroupRegistry& registry = SystemGroupRegistry::Get();
    
    // ... 其他 group ...
    
    // 注册 Particle group
    SystemGroup particle_group("Particle", 
                              ExecutionPhase::RenderCollect, 
                              ExecutionPhase::RenderStat);
    registry.RegisterGroup(particle_group);
    registry.RegisterGroupInstaller("Particle", InstallParticleGroup);
}
```

### 6.3 在 Context 中启用/禁用管道

```cpp
// 启用某个 group
context->SetSystemGroupEnabled("Particle", true);
// → 自动创建 ParticleRenderPipeline
// → 自动注册三个 System

// 禁用某个 group  
context->SetSystemGroupEnabled("Particle", false);
// → 自动调用 pipeline->Shutdown()
// → 自动注销三个 System
// → 自动清理 render_pipelines["Particle"]
```

---

## Context 接口清理

### 7.1 现有混乱的接口

```cpp
class ECSContext {
public:
    // ❌ 特定元素的 getter（需要删除）
    PrimitiveBatchPipeline* GetPrimitiveBatchPipeline();
    TextRenderPipeline* GetTextRenderPipeline();
    // 没有 GetLineRenderPipeline() 和 GetQuadRenderPipeline()
    // → 不一致，容易出错
};
```

### 7.2 目标接口

```cpp
class ECSContext {
public:
    // ✅ 统一的 Pipeline 管理接口
    RenderPipelineBase* GetRenderPipeline(const std::string& name);
    void RegisterRenderPipeline(const std::string& name, 
                               std::unique_ptr<RenderPipelineBase> pipeline);
    bool IsRenderPipelineEnabled(const std::string& name) const;
    std::vector<std::string> GetRenderPipelineNames() const;
    
    // ✅ 不应该再有任何针对特定元素的 API
    // ❌ GetPrimitiveBatchPipeline() - REMOVED
    // ❌ GetTextRenderPipeline() - REMOVED
};
```

### 7.3 迁移的全局调用点

需要找到并更新所有 `GetPrimitiveBatchPipeline()` 和 `GetTextRenderPipeline()` 的调用：

```bash
# 查找 Primitive 相关调用
grep -r "GetPrimitiveBatchPipeline" src/ inc/
# 预期结果：
#   RenderPrimitiveCullSystem.cpp
#   RenderPrimitiveSortSystem.cpp
#   RenderPrimitiveBatchBuildSystem.cpp
#   RenderPrimitiveBatchFinalizeSystem.cpp

# 查找 Text 相关调用
grep -r "GetTextRenderPipeline" src/ inc/
# 预期结果：
#   TextCollectSystem.cpp
#   TextResourceSyncSystem.cpp
#   TextBuildSystem.cpp
```

全部替换为 `GetRenderPipeline("Primitive")` 或 `GetRenderPipeline("Text")`。

---

## 时间表

### Phase 1: 基础设施（已完成 ✅）
- **状态**: ✅ 完成
- **时间**: 2026-02-23
- **交付物**:
  - RenderPipelineBase.h
  - Context 中的统一 Pipeline 注册/查询接口

### Phase 2a: Primitive 迁移（推荐优先）
- **预计时间**: 3-4 小时
- **关键人物**: Render 系统专家
- **交付物**:
  - PrimitiveBatchPipeline 继承 RenderPipelineBase
  - 4 个 System 改造
  - 编译测试通过
  - GizmoUsageExample 测试通过

### Phase 2b: Text 迁移
- **预计时间**: 2-3 小时（基于 2a 的经验）
- **关键人物**: Text 系统专家
- **交付物**:
  - TextRenderPipeline 继承 RenderPipelineBase
  - 3 个 System 改造
  - 编译测试通过
  - Text 相关示例测试通过

### Phase 2c: Line 迁移
- **预计时间**: 4-5 小时（分离 Pipeline 需要更多工作）
- **关键人物**: Render 系统专家
- **交付物**:
  - LineRenderPipeline 新建
  - LineRenderSystem 拆分为 3 个 System
  - 编译测试通过
  - 线条渲染示例测试通过

### Phase 2d: Quad 迁移
- **预计时间**: 3-4 小时
- **关键人物**: Effects 系统专家
- **交付物**:
  - QuadRenderPipeline 新建
  - System 改造
  - 编译测试通过

### Phase 3: Context 接口清理
- **预计时间**: 2-3 小时
- **关键人物**: 架构师
- **交付物**:
  - Context.h/cpp 修改
  - 全局编译测试通过
  - 所有示例正常运行

### Phase 4: 文档和示例
- **预计时间**: 2-3 小时
- **交付物**:
  - 开发者指南更新
  - ParticleRenderPipeline 示例代码
  - 性能对比报告

**总计**: 约 2-3 周（取决于并行度）

---

## 风险评估

### 8.1 高风险项

| 风险 | 概率 | 影响 | 缓解策略 |
|------|------|------|---------|
| **Line 分离失败** | 高 | 高 | 确保有 Line 系统专家参与；需要理解 LineRenderManager 的完整逻辑 |
| **Quad 系统复杂性** | 中 | 高 | 提前审查 QuadResourcePrepareSystem 和相关 System 的代码 |
| **全局编译** | 中 | 高 | 增量迁移，每个阶段立即编译测试 |

### 8.2 中风险项

| 风险 | 概率 | 影响 | 缓解策略 |
|------|------|------|---------|
| **System 执行顺序混乱** | 中 | 中 | 明确每个 System 的 ExecutionPhase；单元测试 |
| **性能回退** | 低 | 中 | 性能基准测试 |

### 8.3 低风险项

| 风险 | 概率 | 影响 | 缓解策略 |
|------|------|------|---------|
| **API 兼容性** | 低 | 低 | 新接口与旧接口并存期间，旧接口可调用新接口 |

---

## 验收标准

### 9.1 编译和构建

- [ ] 整个项目编译通过（Debug + Release）
- [ ] 无警告 (warnings as errors)
- [ ] 每个阶段增量编译通过

### 9.2 功能测试

**Phase 2a - Primitive**:
- [ ] GizmoUsageExample 运行正常，gizmo 正确显示
- [ ] 06_AABBVisualization 运行正常，AABB 框显示
- [ ] Primitive 渲染性能无明显下降

**Phase 2b - Text**:
- [ ] Text 相关示例正常显示
- [ ] 文字批处理有效

**Phase 2c - Line**:
- [ ] LineRenderSystem 拆分后线条正常显示
- [ ] 线条性能无下降

**Phase 2d - Quad**:
- [ ] Billboard/Quad 相关示例正常工作

**Phase 3 - 清理**:
- [ ] Context 中完全移除特定元素的 getter
- [ ] 所有调用处都改用 GetRenderPipeline()
- [ ] 编译无错误

### 9.3 代码质量

- [ ] 无重复代码（DRY 原则）
- [ ] 所有 Pipeline 都继承 RenderPipelineBase，实现相同接口
- [ ] System 之间没有 Pipeline 的具体类型耦合
- [ ] 代码覆盖率 > 80%（可测试部分）

### 9.4 文档

- [ ] 本重构计划文档完整
- [ ] 开发者指南更新
- [ ] ParticleRenderPipeline 示例代码完整可运行
- [ ] 迁移指南明确

---

## 附录：相关定义

### A.1 ExecutionPhase 回顾

```cpp
enum class ExecutionPhase : int {
    // Tick 阶段
    TickInput = 0,
    TickTransform = 1,
    TickCamera = 2,
    TickPostCamera = 3,
    
    // Render 准备阶段（BeginRenderPass 之前）
    RenderSwapchainNextImage = 4,
    RenderPreBeginFrame = 5,
    RenderResourceSetup = 6,
    RenderMaterialBind = 7,
    RenderBeginFrame = 8,
    
    // Render 数据收集和处理阶段
    RenderCollect = 10,       // Pipeline::PrepareFrame() + RunCollect()
    RenderCull = 11,          // Pipeline::RunCull()
    RenderSort = 11,          // Pipeline::RunSort()（可与 RunCull 同阶段）
    RenderBatch = 12,         // Pipeline::RunBuild()
    RenderBufferCommit = 13,  // （框架处理）
    RenderBufferUpload = 14,  // （框架处理）+ Pipeline::RunSync()
    RenderFrameSync = 15,     // （框架处理）
    
    // Render 绘制阶段（BeginRenderPass 内）
    RenderDrawSubmit = 16,    // Pipeline::GetRenderPrimitives() + Render()
    RenderPostProcess = 17,
    RenderDebug = 18,
    RenderStat = 19,
    RenderSubmit = 20,
};
```

### A.2 SystemGroup 的角色

```cpp
struct SystemGroup {
    std::string name;              // "Primitive" / "Text" / "Line" / "Quad"
    ExecutionPhase startPhase;     // 通常是 RenderCollect
    ExecutionPhase endPhase;       // 通常是 RenderStat (包含所有 Collect..Draw 阶段)
    bool enabled;                  // 是否启用这个组
};

// SystemGroup 与 Pipeline 的关系：
// - 每个 SystemGroup 对应一个 Pipeline 实现
// - SystemGroup.enabled = true  ⟹ Pipeline 已创建并注册
// - SystemGroup.enabled = false ⟹ Pipeline 已销毁
```

### A.3 典型的 System 链式调用

```
SystemGroup "Primitive" (RenderCollect → RenderStat)
├─ RenderPrimitiveCollectSystem (RenderCollect)
│  └─ GetPipeline("Primitive") → RunCollect()
├─ RenderPrimitiveCullSystem (RenderCull)
│  └─ GetPipeline("Primitive") → RunCull()
├─ RenderPrimitiveSortSystem (RenderSort)
│  └─ GetPipeline("Primitive") → RunSort()
├─ RenderPrimitiveBatchBuildSystem (RenderBatch)
│  └─ GetPipeline("Primitive") → RunBuild()
├─ RenderPrimitiveBatchFinalizeSystem (RenderBatch, 晚于 Build)
│  └─ GetPipeline("Primitive") → （可选的后续处理）
└─ RenderPrimitiveSubmitSystem (RenderDrawSubmit)
   ├─ GetPipeline("Primitive") → GetRenderPrimitives()
   ├─ RenderDrawCmd 录制绘制命令
   └─ GetPipeline("Primitive") → Render(cmd) （如果 Pipeline 有自定义绘制逻辑）
```

---

## 总结

这个重构计划旨在：

1. **统一所有渲染管道**的实现模式，消除当前混乱的三种不同实现模式
2. **隔离 Context 接口**，使其不再依赖任何特定的渲染元素实现
3. **降低新增渲染类型的成本**，只需实现一个 Pipeline 派生类和对应的 System
4. **提高代码的可测试性和可维护性**

通过这个重构，ULRE 的 ECS 渲染子系统将变得更加清晰、一致、易于扩展。未来无论添加粒子、后处理、UI 还是其他渲染元素，都可以按照统一的 Pipeline 模式，而无需再改动 Context、现有 System 或 Pipeline 的基础设施。
