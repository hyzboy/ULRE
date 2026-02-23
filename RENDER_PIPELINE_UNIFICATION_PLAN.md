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

**Primitive 渲染**:
```
ECSContext::GetPrimitiveBatchPipeline() 
├─ 返回 PrimitiveBatchPipeline*
└─ 被 4 个 System 分别调用:
   ├─ RenderPrimitiveCullSystem::Update()
   ├─ RenderPrimitiveSortSystem::Update()
   ├─ RenderPrimitiveBatchBuildSystem::Update()
   └─ RenderPrimitiveBatchFinalizeSystem::Update()
```

**Text 渲染**:
```
ECSContext::GetTextRenderPipeline()
├─ 返回 TextRenderPipeline*
└─ 被 3 个 System 分别调用:
   ├─ TextCollectSystem::Update()
   ├─ TextResourceSyncSystem::Update()
   └─ TextBuildSystem::Update()
```

**Line 渲染**:
```
LineRenderSystem (自身就是 System)
├─ 持有 LineRenderManager* 
├─ Update() 在 RenderBatch 阶段
└─ Render() 在 RenderDrawSubmit 阶段
└─ 不经过 Pipeline 模式
```

**Quad/Billboard 渲染**:
```
QuadRenderSystem / BillboardSystem (自身就是 System)
├─ 持有相应 Manager
└─ 不经过 Pipeline 模式
```

**现象**: 三种不同的模式混在一起，难以理解、维护和扩展。

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

- **Primitive/Text**: System 是 Pipeline 的客户端，Pipeline 是共享资源（Provider 模式）
- **Line/Quad**: System 本身就包含 Pipeline 逻辑（Monolith 模式）
- 没有统一的接口约定

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

#### 4.2.2 每个 Pipeline 的设计

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

## 具体迁移步骤

### 5.1 迁移顺序和依赖

```
┌─────────────────────────────────────────────────┐
│ Phase 1: 基础设施 (已完成)                       │
│ - RenderPipelineBase                           │
│ - Context::RegisterRenderPipeline()            │
│ - Context::GetRenderPipeline()                 │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│ Phase 2a: Primitive 迁移 (推荐首先)              │
│ - PrimitiveBatchPipeline implements Base      │
│ - 4 个 System 改用新模式                        │
│ - 测试完整性                                    │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│ Phase 2b: Text 迁移 (同步进行)                   │
│ - TextRenderPipeline implements Base           │
│ - 3 个 System 改用新模式                        │
│ - 测试完整性                                    │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│ Phase 2c: Line 迁移 (同步进行)                   │
│ - 从 LineRenderSystem 中分离 LineRenderPipeline │
│ - 创建 LineCollectSystem, LineRenderSystem     │
│ - 测试完整性                                    │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│ Phase 2d: Quad/Billboard 迁移                    │
│ - 从 QuadResourcePrepareSystem 中分离           │
│ - 创建 QuadRenderPipeline                      │
│ - 创建对应的 System                             │
│ - 测试完整性                                    │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│ Phase 3: Context 接口清理                       │
│ - 删除 GetPrimitiveBatchPipeline()             │
│ - 删除 GetTextRenderPipeline()                 │
│ - 更新所有调用处                                │
│ - 全局编译测试                                  │
└─────────────────────────────────────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
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

当前 LineRenderSystem 是单一的 System，同时管理 Batch 和 Draw 逻辑。

需要拆分为：
1. **LineRenderPipeline**: 维护行渲染数据、批处理
2. **LineCollectSystem**: Collect 阶段（继承 CollectSystem）
3. **LineBatchSystem**: Build 阶段（继承 BuildSystem）
4. **LineRenderSystem**: Draw 阶段（继承 RenderPipelineDrawSystem）

```cpp
// inc/hgl/ecs/support/LineRenderPipeline.h
class LineRenderPipeline : public RenderPipelineBase {
    // 从 LineRenderSystem 中的 LineRenderManager 迁移过来
};

// inc/hgl/ecs/systems/render/LineCollectSystem.h
class LineCollectSystem : public CollectSystem {
    RenderPipelineBase* GetPipeline(ECSContext* ctx) override {
        return ctx->GetRenderPipeline("Line");
    }
    void OnCollect(RenderPipelineBase* pipeline) override { ... }
};

// 同理: LineBatchSystem, LineRenderSystem 改造
```

### 5.5 Phase 2d 详细步骤 - Quad 迁移

类似 Line 迁移，需要分离 QuadRenderPipeline 和相应的 System。

### 5.6 Phase 3 - Context 接口清理

#### Step 1: 删除特定元素的 getter

在 `inc/hgl/ecs/core/Context.h` 中：

```cpp
// 删除这些函数
PrimitiveBatchPipeline* GetPrimitiveBatchPipeline();    // REMOVE
TextRenderPipeline* GetTextRenderPipeline();            // REMOVE

// 所有访问统一为：
RenderPipelineBase* GetRenderPipeline(const std::string& name);
```

#### Step 2: 更新所有调用处

查找并替换：

```cpp
// 之前
context->GetPrimitiveBatchPipeline()->RunCulling();

// 之后
auto p = context->GetRenderPipeline("Primitive");
if (p) p->RunCull();  // 注: 通过虚接口调用
```

#### Step 3: 删除成员变量

在 `inc/hgl/ecs/core/Context.h` 的私有部分：

```cpp
// 删除这些成员
std::unique_ptr<PrimitiveBatchPipeline> primitive_batch_pipeline;
std::unique_ptr<TextRenderPipeline> text_render_pipeline;

// 只保留统一的 map：
std::unordered_map<std::string, std::unique_ptr<RenderPipelineBase>> render_pipelines;
```

#### Step 4: 更新 Context 的 Shutdown()

```cpp
void ECSContext::Shutdown() {
    // 之前的特定清理逻辑可以删除
    // text_render_pipeline.reset();        // REMOVE
    // primitive_batch_pipeline.reset();    // REMOVE
    
    // 统一清理
    for (auto& [name, pipeline] : render_pipelines) {
        if (pipeline)
            pipeline->Shutdown();
    }
    render_pipelines.clear();
}
```

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
