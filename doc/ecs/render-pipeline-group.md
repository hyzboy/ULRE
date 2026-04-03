# RenderPipelineGroup 体系说明

> 涉及文件：
> - `inc/hgl/ecs/support/RenderPipelineBase.h`
> - `inc/hgl/ecs/support/RenderPipelineSystem.h`
> - `inc/hgl/ecs/support/RenderPipelineGroup.h`
> - `inc/hgl/ecs/core/SystemGroup.h`
> - `inc/hgl/ecs/core/DefaultSystems.h`
> - `src/ecs/support/RenderPipelineSystem.cpp`
> - `src/ecs/support/RenderPipelineGroup.cpp`
> - `src/ecs/core/DefaultSystems.cpp`
> - `src/ecs/support/primitive/PrimitiveRenderPipelineGroup.cpp`
> - `src/ecs/support/line/LineRenderPipelineGroup.cpp`
> - `src/ecs/support/text/TextRenderPipelineGroup.cpp`
> - `src/ecs/support/billboard/BillboardRenderPipelineGroup.cpp`
> - `src/ecs/support/terrain/TerrainRenderPipelineGroup.cpp`

---

## 1. 背景与设计目标

在早期实现中，每种渲染类型（图元、线段、文字……）各自独立地将多个 System 硬编码到 `ECSContext` 的初始化流程中。这导致：

- `ECSContext` 不得不了解每种渲染类型的 System
- 添加新类型时需要修改核心文件
- 难以按需开关某一整类渲染

**RenderPipelineGroup** 架构将每种渲染类型的全部职责打包成一个自包含单元：

```
RenderPipelineGroup
├── RenderPipelineBase    ← 业务逻辑（数据、阶段方法）
└── RenderPipelineSystem  ← 薄代理（ECS Phase → GraphicsPipeline 方法调用）
```

`ECSContext` 只需要知道"名称"，全部细节由 Group 负责。

---

## 2. 核心类层次

### 2.1 `RenderPipelineBase` — 渲染管线抽象基类

所有管线实现的抽象父类，定义了每帧 8 个阶段的虚方法：

| 方法 | 阶段 | 说明 |
|------|------|------|
| `PrepareFrame()` | pre-Collect | 初始化每帧状态，返回 false 表示本帧无可见内容 |
| `RunCollect()` | `RenderCollect` | 收集可见组件 |
| `RunCull()` | `RenderCull`（可选）| 视锥/遮挡剔除 |
| `RunSort()` | `RenderSort`（可选）| 深度/优先级排序 |
| `RunBuild()` | `RenderBatch` | 构建批次，写 VAB/ICB |
| `RunSync()` | `RenderFrameSync`（可选）| 上传后同步 UBO/Descriptor |
| `GetRenderPrimitives(out)` | 查询 | 返回本帧已调度的 Primitive 列表 |
| `Render(cmd)` | `RenderDrawSubmit` | 录制 GPU 绘制命令 |

未覆盖的可选阶段（`RunCull`/`RunSort`/`RunSync`）有空实现，不需要则无需重写。

---

### 2.2 `RenderPipelineSystem` — 薄代理 System 基类

```
RenderPipelineSystem          ← System 子类，含 ValidatePipeline()
├── CollectSystem             → OnCollect(pipeline)
├── CullSystem                → OnCull(pipeline)
├── SortSystem                → OnSort(pipeline)
├── BuildSystem               → OnBuild(pipeline)
├── SyncSystem                → OnSync(pipeline)
└── RenderPipelineDrawSystem  → OnRender(pipeline, cmd)
```

每个阶段类的 `Update()` / `Render()` 执行如下固定流程：

```cpp
// 以 BuildSystem 为例（src/ecs/support/RenderPipelineSystem.cpp）
void BuildSystem::Update(float dt)
{
    if (!ValidatePipeline(context))   // ① 检查 enabled + context + pipeline
        return;
    auto pipeline = GetPipeline(context);
    OnBuild(pipeline);                // ② 调用派生类实现
}
```

`GetPipeline(context)` 由具体 System 类实现，通常只有一行：

```cpp
// src/ecs/support/primitive/PrimitiveBuildSystem.cpp
RenderPipelineBase* PrimitiveBuildSystem::GetPipeline(ECSContext* context)
{
    return context->GetRenderPipeline("Primitive");
}

void PrimitiveBuildSystem::OnBuild(RenderPipelineBase* pipeline)
{
    pipeline->RunBuild();
}
```

---

### 2.3 `RenderPipelineGroup` — 容器基类

```cpp
class RenderPipelineGroup
{
protected:
    std::string                                     name_;
    std::unique_ptr<RenderPipelineBase>             pipeline_;   // 逻辑所有者
    std::vector<std::unique_ptr<RenderPipelineSystem>> systems_; // 系统所有者
    bool                                            enabled_ = true;

public:
    virtual bool Initialize(ECSContext* context) = 0;
    virtual void Shutdown(ECSContext* context)   = 0;

protected:
    virtual std::unique_ptr<RenderPipelineBase> CreatePipeline() = 0;
    virtual void RegisterSystems() = 0;
};
```

**关键设计说明**

1. `pipeline_` / `systems_` 存储在 Group 内，但 `Initialize()` 同时将它们注册到 `ECSContext`。Context 持有 `shared_ptr` 或直接持有，因此 **Group 析构不等于资源消失**——Shutdown 之前 Context 仍可正常使用它们。
2. 大多数派生类将 GraphicsPipeline 和 System 的创建都写在 `Initialize()` 中（因为需要 `context` 参数），`CreatePipeline()` 和 `RegisterSystems()` 保持空实现（仅供特殊扩展场景使用）。

---

## 3. 派生实现一览

| Group 类 | 注册名 | 管线类 | 注册系统（有序） |
|--|--|--|--|
| `PrimitiveRenderPipelineGroup` | `"Primitive"` | `PrimitiveRenderPipeline` | Cull → Sort → Build → Render → OverlayRender |
| `LineRenderPipelineGroup` | `"Line"` | `LineRenderPipeline` | Collect → Build → Render |
| `TextRenderPipelineGroup` | `"Text"` | `TextRenderPipelineAdapter`（适配器） | Collect → Build → Sync → Render |
| `BillboardRenderPipelineGroup` | `"Billboard"` | *无独立管线* | `QuadResourcePrepareSystem` → `QuadMaterialBindingSystem` |
| `TerrainRenderPipelineGroup` | `"Terrain"` | `TerrainRenderPipeline` | Collect → Build → Render |

---

## 4. 各派生实现详解

### 4.1 `PrimitiveRenderPipelineGroup`

图元渲染是 ECS 中最复杂的管线，承载网格/材质/Transform 的全套批次逻辑。

```
Initialize(context)
├── 创建 PrimitiveRenderPipeline(context)
│     └── 内部包装 PrimitiveBatchPipeline（完整批次+VAB写入逻辑）
├── RegisterRenderSystem<PrimitiveCullSystem>      RenderBatch
├── RegisterRenderSystem<PrimitiveSortSystem>      RenderBatch
├── RegisterRenderSystem<PrimitiveBuildSystem>     RenderBatch
├── RegisterRenderSystem<PrimitiveRenderSystem>    RenderDrawSubmit
└── RegisterRenderSystem<PrimitiveOverlayRenderSystem> RenderDrawSubmit
```

**注意**：`RenderPrimitiveCollectSystem` **不在**此 Group 内，由 `InstallPrimitiveGroup` 单独注册（它是 Collect 阶段的共享系统，同时也为 Billboard 服务）。

---

### 4.2 `LineRenderPipelineGroup`

线段渲染的三段式对称结构。

```
Initialize(context)
├── 创建 LineRenderPipeline(context)
├── RegisterRenderSystem<LineCollectSystem>        RenderCollect
├── RegisterRenderSystem<LineBuildSystem>          RenderBatch
└── RegisterRenderSystem<LineRenderSystem>         RenderDrawSubmit
```

`LineBoundsUpdateSystem`（Tick 阶段，更新 AABB）**不在**此 Group 内，由 `InstallLineGroup` 在 Group 安装前单独注册为 TickSystem。

---

### 4.3 `TextRenderPipelineGroup`

文字渲染使用适配器模式，将 Context 中已有的 `TextRenderPipeline`（由字体系统管理）包装为 `RenderPipelineBase` 接口。

```
Initialize(context)
├── 创建 TextRenderPipelineAdapter(context)   ← 包装 context 内部的 TextRenderPipeline
├── RegisterRenderSystem<TextCollectSystem>    RenderCollect
├── RegisterRenderSystem<TextBuildSystem>      RenderBatch
├── RegisterRenderSystem<TextSyncSystem>       RenderFrameSync   ← 字形纹理 Atlas 上传同步
└── RegisterRenderSystem<TextRenderSystem>     RenderDrawSubmit
```

`TextSyncSystem`（`RenderFrameSync` 阶段）是 Text 特有的，用于在 GPU 上传字形 Atlas 后同步描述符。

---

### 4.4 `BillboardRenderPipelineGroup`

Billboard（公告板/Billboard Quad）**没有独立的渲染管线**。其几何体通过 `RenderPrimitiveCollectSystem` 收集后流入 Primitive 管线完成绘制。该 Group 仅负责两项资源准备工作：

```
Initialize(context)
├── RegisterRenderSystem<QuadResourcePrepareSystem>   RenderResourceSetup
│     └── 懒初始化 Quad VBO/IBO/材质
└── RegisterRenderSystem<QuadMaterialBindingSystem>   RenderMaterialBind
      └── 将 Texture/Sampler 绑定到 Billboard 材质实例
```

无 pipeline_ 注册，`CreatePipeline()` 返回 `nullptr`。

---

### 4.5 `TerrainRenderPipelineGroup`

地形渲染无 VBO/IBO，使用 Indirect Draw，是最简洁的三段式 Group。

```
Initialize(context)
├── 创建 TerrainRenderPipeline(context)        ← 名字常量 TerrainRenderPipeline::kName = "Terrain"
├── RegisterRenderSystem<TerrainCollectSystem>  RenderCollect
├── RegisterRenderSystem<TerrainBuildSystem>    RenderBatch   ← 写 Indirect Buffer
└── RegisterRenderSystem<TerrainRenderSystem>   RenderDrawSubmit ← vkCmdDrawIndirect
```

---

## 5. SystemGroupRegistry 与 DefaultSystems 注册机制

### 5.1 整体流程

```
                ┌──────────────────────────────────┐
                │      SystemGroupRegistry（单例）   │
                │  map<name, installer_fn>          │
                └──────────┬───────────────────────┘
                           │ RegisterBuiltinSystemGroupInstallers()
                           │（首次调用 EnsureCoreEcsSystems 时自动执行，只执行一次）
                           ▼
   "Primitive" → InstallPrimitiveGroup()
   "Text"      → InstallTextGroup()
   "Billboard" → InstallBillboardGroup()
   "Line"      → InstallLineGroup()
   "Terrain"   → InstallTerrainGroup()
```

### 5.2 懒安装：`EnsureSystemGroupSystems()`

```cpp
bool EnsureSystemGroupSystems(ECSContext* ctx, const std::string& group_name, IRenderTarget* rt)
{
    EnsureCoreEcsSystems(ctx, rt);              // 保证核心系统先存在

    if (ctx->IsSystemGroupInstalled(group_name)) // 幂等：已安装则直接返回
        return true;

    auto& registry = SystemGroupRegistry::Get();
    registry.EnsureGroupSystems(group_name, ctx, rt);  // 调用 installer_fn

    ctx->MarkSystemGroupInstalled(group_name);  // 标记，防止重复安装
    ctx->GetSystemProfiler().MarkGroupEnsured(group_name);
    return true;
}
```

**幂等保证**：同一 Group 无论调用多少次 `EnsureSystemGroupSystems`，其 Systems 和 GraphicsPipeline 只会注册一次。

### 5.3 `RegisterDefaultEcsSystems()` — 便捷函数

```cpp
DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext* ctx, IRenderTarget* rt)
{
    EnsureCoreEcsSystems(ctx, rt);              // Input, Camera, Environment, RenderTarget …
    EnsureSystemGroupSystems(ctx, "Primitive", rt);
    EnsureSystemGroupSystems(ctx, "Text", rt);
    EnsureSystemGroupSystems(ctx, "Billboard", rt);
    EnsureSystemGroupSystems(ctx, "Line", rt);
    // "Terrain" 按需调用，不含在默认集中
    return { … };   // 返回常用 System 的 weak_ptr 便于调用方缓存
}
```

---

## 6. 帧执行顺序与各 Group 的阶段分布

```
ExecutionPhase            系统来源
─────────────────────────────────────────────────────────────────
TickInput                 InputSystem
TickCamera                CameraSystem
RenderSwapchainNextImage  SwapchainNextImageSystem
RenderPreBeginFrame       EnvironmentSystem  /  RenderTargetSystem
RenderResourceSetup       QuadResourcePrepareSystem  [Billboard]
RenderMaterialBind        QuadMaterialBindingSystem  [Billboard]
RenderBeginFrame          (RenderSystemCore 内部)
RenderCollect             RenderPrimitiveCollectSystem  [Primitive]
                          LineCollectSystem             [Line]
                          TextCollectSystem             [Text]
                          TerrainCollectSystem          [Terrain]
RenderBatch               PrimitiveCullSystem           [Primitive]
                          PrimitiveSortSystem           [Primitive]
                          PrimitiveBuildSystem          [Primitive]
                          LineBuildSystem               [Line]
                          TextBuildSystem               [Text]
                          TerrainBuildSystem            [Terrain]
RenderBufferUpload        RenderBufferUploadSystem
RenderFrameSync           TextSyncSystem                [Text]
                          RenderDescriptorBindingSystem
RenderDrawSubmit          PrimitiveRenderSystem         [Primitive]
                          PrimitiveOverlayRenderSystem  [Primitive]
                          LineRenderSystem              [Line]
                          TextRenderSystem              [Text]
                          TerrainRenderSystem           [Terrain]
RenderStat                LineStatsSystem
RenderSubmit              SwapchainSubmitSystem
```

---

## 7. 新增一个 RenderPipelineGroup 的步骤

以添加 "Particle" 粒子管线为例：

### 步骤 1：实现 `RenderPipelineBase`

```cpp
// inc/hgl/ecs/support/particle/ParticleRenderPipeline.h
class ParticleRenderPipeline : public RenderPipelineBase
{
public:
    explicit ParticleRenderPipeline(ECSContext* ctx);
    const std::string& GetName() const override { return kName; }
    ECSContext* GetWorld() const override { return ctx_; }

    bool PrepareFrame() override;
    void RunCollect() override;
    void RunBuild() override;
    void Render(RenderCmdBuffer* cmd) override;
    void GetRenderPrimitives(std::vector<Primitive*>& out) const override;

    static inline const std::string kName = "Particle";
private:
    ECSContext* ctx_;
};
```

### 步骤 2：实现各阶段 System

每个阶段对应一个薄代理类：

```cpp
// ParticleCollectSystem.h
class ParticleCollectSystem : public CollectSystem
{
public:
    RenderPipelineBase* GetPipeline(ECSContext* context) override
    {
        return context->GetRenderPipeline("Particle");
    }
private:
    void OnCollect(RenderPipelineBase* pipeline) override
    {
        pipeline->RunCollect();
    }
};
```

在构造函数中设置 ExecutionPhase：

```cpp
ParticleCollectSystem::ParticleCollectSystem()
    : CollectSystem("ParticleCollectSystem")
{
    SetExecutionOrder(ExecutionPhase::RenderCollect);
    SetRenderElementType("Particle");
}
```

### 步骤 3：实现 `RenderPipelineGroup`

```cpp
// ParticleRenderPipelineGroup.cpp
bool ParticleRenderPipelineGroup::Initialize(ECSContext* context)
{
    auto pipeline = std::make_unique<ParticleRenderPipeline>(context);
    context->RegisterRenderPipeline(name_, std::move(pipeline));

    context->RegisterRenderSystem<ParticleCollectSystem>();
    context->RegisterRenderSystem<ParticleBuildSystem>();
    context->RegisterRenderSystem<ParticleRenderSystem>();
    return true;
}
```

### 步骤 4：在 `DefaultSystems.cpp` 注册

```cpp
// 在 RegisterBuiltinSystemGroupInstallers() 中添加
static bool InstallParticleGroup(ECSContext* ctx, IRenderTarget*)
{
    ParticleRenderPipelineGroup group;
    return group.Initialize(ctx);
}

// 在函数体中：
registry.RegisterGroupInstaller("Particle", InstallParticleGroup);
```

### 步骤 5：按需调用

```cpp
// 在场景初始化时：
EnsureSystemGroupSystems(ctx, "Particle", render_target);
```

---

## 8. 设计总结

| 关注点 | 实现方式 |
|--------|---------|
| ECSContext 无需了解具体渲染类型 | Group 持有 GraphicsPipeline+System，通过名称字符串注册 |
| System 只做调度，不含业务逻辑 | 每个 System 只有 6 行：`GetPipeline()`+`OnXxx()` |
| 新渲染类型不修改核心文件 | 只需新建 Group + 在 DefaultSystems 注册 installer |
| 整类渲染可按需开关 | `SystemGroupRegistry::SetGroupEnabled(name, false)` |
| 幂等安装 | `EnsureSystemGroupSystems` + `IsSystemGroupInstalled` 双重保护 |
| 线外系统（Tick/Stats）不进 Group | `LineBoundsUpdateSystem`、`LineStatsSystem` 由 `InstallLineGroup` 单独注册 |
