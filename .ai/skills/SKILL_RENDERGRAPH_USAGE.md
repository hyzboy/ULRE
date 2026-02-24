# SKILL: RenderGraph使用和自定义渲染流程

## 目标
学会使用RenderGraph定义和切换渲染流程，支持多RT（渲染目标）场景、质量预设、和自定义渲染策略。

## RenderGraph基础

### 概念：Pass（通道）

一个Pass是一个原子渲染操作单元：
```cpp
struct RenderGraph::Pass {
    ExecutionPhase startPhase;        // 从哪个phase开始
    ExecutionPhase endPhase;          // 到哪个phase结束
    RenderTarget* renderTarget;       // 渲染目标（nullptr=当前RT）
    bool enabled;                     // 是否启用
    bool runUpdate;                   // 是否运行 System::Update()
    bool submitTransforms;            // 是否调用 SubmitTransformUpdates()
    bool runRender;                   // 是否运行 System::Render()
};
```

### 三个预设工厂

```cpp
// 1. 默认线性图：一个Pass覆盖所有阶段
RenderGraph graph = CreateDefaultLinearGraph();

// 2. 自适应图：自动检测场景内容，启用/禁用系统组
RenderGraph graph = CreateAdaptiveRenderGraph(context);

// 3. 自定义图：手动构建
RenderGraph custom_graph;
custom_graph.Add(RenderGraph::Pass(...));
custom_graph.Add(RenderGraph::Pass(...));
```

---

## 使用RenderGraph渲染

### 基本用法

```cpp
// 方式1：使用默认自适应图
context->Render(deltaTime);  // 内部使用CreateAdaptiveRenderGraph(this)

// 方式2：指定自己的图
RenderGraph graph = CreateMainWithLineOverlayGraph();
context->Render(deltaTime, graph);

// 方式3：带回调的渲染
RenderGraph graph = CreateAdaptiveRenderGraph(context);
context->Render(deltaTime, graph, [](RenderCmdBuffer* cmd) {
    // 在所有Pass完成后调用
    // 用于自定义后处理等
});
```

### 完整场景示例

```cpp
// 初始化
ECSContext world;
world.Initialize();

// 注册系统
SetupDefaultSystems(world);

// 主渲染循环
float deltaTime = 0.016f;
while (running) {
    // 更新逻辑
    world.Tick(deltaTime);
    
    // 渲染
    RenderGraph graph = CreateAdaptiveRenderGraph(&world);
    world.Render(deltaTime, graph);
    
    deltaTime = timer.GetDeltaTime();
}
```

---

## 预设RenderGraph

### 1. 默认线性图

```cpp
RenderGraph CreateDefaultLinearGraph() {
    RenderGraph graph;
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderPostProcess,
        nullptr,  // 使用当前RT
        true,     // enabled
        true,     // runUpdate
        true,     // submitTransforms
        true      // runRender
    ));
    return graph;
}
```

**特点：**
- 最简单的流程
- 所有系统都会执行
- 用于调试或简单场景

### 2. 主场景图（带线叠加）

```cpp
RenderGraph CreateMainWithLineOverlayGraph() {
    RenderGraph graph;
    
    // Pass1: 主渲染（Primitive + Text）
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderDrawSubmit,
        nullptr,
        true, true, true, true
    ));
    
    // Pass2: 线叠加（后处理阶段）
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderPostProcess,
        ExecutionPhase::RenderPostProcess,
        nullptr,
        true, false, false, true  // 只运行Render
    ));
    
    return graph;
}
```

**特点：**
- 保证线条始终在最前
- 通过两个Pass精细控制
- RenderPostProcess 包含 LineRenderPipelineGroup


### 3. 仅线条图

```cpp
RenderGraph CreateLineOnlyGraph() {
    RenderGraph graph;
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderPostProcess,
        ExecutionPhase::RenderPostProcess,
        nullptr,
        true, true, true, true
    ));
    return graph;
}
```

**特点：**
- 调试用：只显示线条
- 用于验证线条渲染逻辑

### 4. 自适应图

```cpp
RenderGraph CreateAdaptiveRenderGraph(ECSContext* context) {
    // 扫描场景，检测内容
    SceneStats stats = GatherSceneStats(context);
    
    // 根据内容启用/禁用系统
    context->SetElementTypeSystemsEnabled("Primitive", stats.hasPrimitives);
    context->SetElementTypeSystemsEnabled("Text", stats.hasText);
    context->SetElementTypeSystemsEnabled("Line", stats.hasLines);
    context->SetElementTypeSystemsEnabled("Billboard", stats.hasBillboards);
    
    // 创建单一Pass（系统内部过滤）
    RenderGraph graph;
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderPostProcess,
        nullptr, true, true, true, true
    ));
    
    return graph;
}
```

**特点：**
- 自动检测场景内容
- 禁用不需要的系统组
- 降低CPU/GPU成本

---

## 自定义RenderGraph

### 场景1：多RT渲染（G-Buffer）

```cpp
RenderGraph CreateGBufferGraph(
    ECSContext* context,
    RenderTarget* rt_position,
    RenderTarget* rt_normal,
    RenderTarget* rt_albedo)
{
    RenderGraph graph;
    
    // Pass1: 渲染到Position RT
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderDrawSubmit,
        rt_position,  // ⭐ 指定RT
        true, true, true, true
    ));
    
    // Pass2: 渲染到Normal RT
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderDrawSubmit,
        rt_normal,
        true, false, false, true  // 复用数据
    ));
    
    // Pass3: 渲染到Albedo RT
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderDrawSubmit,
        rt_albedo,
        true, false, false, true
    ));
    
    return graph;
}
```

### 场景2：质量预设（低/高配）

```cpp
RenderGraph CreateLowQualityGraph(ECSContext* context) {
    // 禁用高成本特性
    context->SetElementTypeSystemsEnabled("Particle", false);
    context->SetElementTypeSystemsEnabled("Decal", false);
    context->SetElementTypeSystemsEnabled("DetailGrass", false);
    
    // 创建简化图
    RenderGraph graph;
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderDrawSubmit,
        nullptr,
        true, true, true, true
    ));
    
    return graph;
}

RenderGraph CreateHighQualityGraph(ECSContext* context) {
    // 启用所有高端特性
    context->SetElementTypeSystemsEnabled("Particle", true);
    context->SetElementTypeSystemsEnabled("Decal", true);
    context->SetElementTypeSystemsEnabled("DetailGrass", true);
    
    // 完整图
    return CreateAdaptiveRenderGraph(context);
}
```

### 场景3：分层渲染（背景→主体→UI）

```cpp
RenderGraph CreateLayeredGraph(ECSContext* context) {
    RenderGraph graph;
    
    // Layer1: 背景（SkySphere 或其他背景元素）
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderPostProcess,
        ExecutionPhase::RenderPostProcess,
        nullptr,
        true, true, false, true
    ));
    
    // Layer2: 主体（Primitive + Text + Billboard + Particle）
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderCollect,
        ExecutionPhase::RenderDrawSubmit,
        nullptr,
        true, true, true, true
    ));
    
    // Layer3: UI叠加（如 Line 或 Text UI）
    graph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderPostProcess,
        ExecutionPhase::RenderPostProcess,
        nullptr,
        true, false, false, true
    ));
    
    return graph;
}
```

---

## Pass执行标志详解

### runUpdate: bool
```cpp
// true: 调用 System::Update()
// 常用于数据收集、CPU端处理

pass.runUpdate = true   // 启用Update
    ↓
for (phase in [startPhase, endPhase)
    for (system in phase)
        if (system->IsEnabled())
            system->Update(deltaTime)
```

### submitTransforms: bool
```cpp
// true: 调用 TransformSystem::SubmitTransformUpdates()
// 将CPU变换同步到GPU

pass.submitTransforms = true
    ↓
TransformSystem::SubmitTransformUpdates()  // GPU缓冲更新
```

### runRender: bool
```cpp
// true: 调用 System::Render(cmd, deltaTime)
// 用于GPU命令提交

pass.runRender = true
    ↓
for (phase in [startPhase, endPhase)
    for (system in phase)
        if (system->IsEnabled())
            system->Render(cmd, deltaTime)
```

### 典型组合

| 用途 | runUpdate | submitTransforms | runRender |
|------|-----------|-----------------|-----------|
| 完整渲染 | ✓ | ✓ | ✓ |
| 仅数据收集 | ✓ | ✗ | ✗ |
| 仅GPU提交 | ✗ | ✗ | ✓ |
| 复用第1 Pass数据 | ✗ | ✗ | ✓ |
| 转向不同RT | ✗ | ✓ | ✗ |

---

## 动态切换RenderGraph

```cpp
class WorldRenderer {
private:
    ECSContext* world;
    enum class Quality { Low, Medium, High, Ultra };
    Quality current_quality = Quality::High;
    
public:
    void SetQuality(Quality level) {
        if (level == current_quality) return;
        
        current_quality = level;
        
        // 重新规划系统配置
        switch (level) {
            case Quality::Low:
                world->SetElementTypeSystemsEnabled("Particle", false);
                world->SetElementTypeSystemsEnabled("Decal", false);
                break;
            case Quality::High:
                world->SetElementTypeSystemsEnabled("Particle", true);
                world->SetElementTypeSystemsEnabled("Decal", true);
                break;
            // ...
        }
        
        // 强制重新生成自适应图
        world->InvalidateAdaptiveRenderGraph();
    }
    
    void Frame(float deltaTime) {
        RenderGraph graph;
        
        switch (current_quality) {
            case Quality::Low:
                graph = CreateLowQualityGraph(world);
                break;
            case Quality::High:
                graph = CreateAdaptiveRenderGraph(world);
                break;
            // ...
        }
        
        world->Render(deltaTime, graph);
    }
};
```

---

## 调试RenderGraph

### 打印Pass序列

```cpp
void DebugRenderGraph(const RenderGraph& graph) {
    printf("=== RenderGraph Debug ===\n");
    
    size_t pass_idx = 0;
    for (const auto& pass : graph.GetPasses()) {
        printf("Pass %zu:\n", pass_idx++);
        printf("  Phase range: %d→%d\n", 
               pass.startPhase, pass.endPhase);
        printf("  Enabled: %s\n", pass.enabled ? "YES" : "NO");
        printf("  Steps: Update:%s Transform:%s Render:%s\n",
               pass.runUpdate ? "Y" : "N",
               pass.submitTransforms ? "Y" : "N",
               pass.runRender ? "Y" : "N");
        printf("  RT: %s\n", pass.renderTarget ? "Custom" : "Current");
    }
}
```

### 运行时性能分析

```cpp
struct FrameStats {
    float tick_time = 0;
    float render_time = 0;
    float pass_times[16] = {0};
};

FrameStats frame_stats;

context->Render(deltaTime, graph, [&](RenderCmdBuffer* cmd) {
    frame_stats.render_time = timer.GetElapsed();
    
    if (frame_stats.render_time > 16.67f) {  // > 60fps
        LogWarning("Frame took %.2fms (slow render)",
                   frame_stats.render_time);
    }
});
```

---

## 最佳实践

✅ **DO:**
- 使用预设RenderGraph（Default/Adaptive/Preset）
- 在Pass之间使用合理的RunUpdate/SubmitTransforms/RunRender组合
- 为不同场景（MainGame/Editor/Preview）创建不同图
- 通过 `SetElementTypeSystemsEnabled()` 控制系统，而非多个Pass

❌ **DON'T:**
- 不要创建过多相似的Pass（应该通过系统的enabled标志过滤）
- 不要在Pass中跨越太多ExecutionPhase（保持range在合理范围）
- 不要忘记在切换质量等级时调用 `InvalidateAdaptiveRenderGraph()`
- 不要为了单个系统创建独立Pass（应该组织成系统组）

---

## 参考代码

- `inc/hgl/ecs/core/RenderGraph.h` - RenderGraph和Pass定义
- `src/ecs/core/RenderGraph.cpp` - 所有预设工厂实现
- `inc/hgl/ecs/core/Context.h` - Render() 重载
- `src/ecs/core/Context.cpp` - Render()实现 (~200行)
