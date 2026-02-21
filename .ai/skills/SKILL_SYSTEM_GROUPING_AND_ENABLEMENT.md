# SKILL: RenderGraph系统分组和启用/禁用管理

## 目标  
理解和使用新的系统分组机制，支持按元素类型动态启用/禁用系统，实现自适应渲染性能优化。

## 核心概念

### Element Type（元素类型）
每个系统通过 `SetRenderElementType(string)` 声明它渲染什么元素类型。

**已有的元素类型：**
- `"Primitive"` - 3D基础几何体
- `"Text"` - 文本渲染
- `"Line"` - 线条渲染
- `"Billboard"` - 广告牌（Quad）

**可添加的元素类型：**
- `"SkySphere"` - 天空球
- `"Particle"` - 粒子系统
- `"Decal"` - 贴花
- `"Terrain"` - 地形
- `"Water"` - 水体

---

## API参考

### 1. 系统端：声明Element Type

```cpp
// 在System构造函数中
MyElementSystem::MyElementSystem(const std::string& name) : System(name) {
    SetSystemType(SystemType::RenderCollect);
    SetExecutionOrder(ExecutionPhase::RenderCollect_MyElementCollectSystem);
    SetRenderElementType("MyElement");  // ⭐ 声明
}

// 查询
const std::string& elementType = system->GetRenderElementType();
```

**特点：**
- 一个系统只能有一个element type
- 相同element type的系统形成逻辑组
- Element type为空字符串表示系统不受自适应图控制

---

### 2. Context端：按名称启用/禁用系统

```cpp
// 启用/禁用某element type的所有系统
context->SetElementTypeSystemsEnabled("Primitive", true);   // 启用所有Primitive系统
context->SetElementTypeSystemsEnabled("Text", false);       // 禁用所有Text系统

// 获取某element type的系统列表
std::vector<std::shared_ptr<System>> systems;
context->GetSystemsByElementType("Primitive", systems);

for (auto& sys : systems) {
    sys->SetEnabled(false);  // 也可逐个控制
}
```

---

### 3. RenderGraph端：自适应场景内容

```cpp
// 创建自适应图（自动检测场景内容）
RenderGraph graph = CreateAdaptiveRenderGraph(context);

// 手动启用/禁用（用于质量预设等）
context->SetElementTypeSystemsEnabled("Particle", quality_level >= Medium);
context->SetElementTypeSystemsEnabled("Terrain", quality_level == Ultra);
```

**自动检测流程：**
```
GatherSceneStats(context)
  ├─ 检查是否有PrimitiveComponent → hasPrimitives
  ├─ 检查是否有TextComponent → hasText
  ├─ 检查是否有BillboardComponent → hasBillboards
  └─ (已关闭) 检查是否有LineComponent → hasLines
  
CreateAdaptiveRenderGraph(context)
  ├─ context->SetElementTypeSystemsEnabled("Primitive", hasPrimitives)
  ├─ context->SetElementTypeSystemsEnabled("Text", hasText)
  ├─ context->SetElementTypeSystemsEnabled("Line", hasLines)
  ├─ context->SetElementTypeSystemsEnabled("Billboard", hasBillboards)
  └─ 创建单一Pass覆盖所有阶段，系统通过enabled标志自己过滤
```

---

## 实战场景

### 场景1：条件渲染（有Content则启用）

```cpp
// 场景检测逻辑
auto scene = world->GetComponent<SceneComponent>();
if (scene->has_terrain) {
    context->SetElementTypeSystemsEnabled("Terrain", true);
} else {
    context->SetElementTypeSystemsEnabled("Terrain", false);
}
```

### 场景2：质量预设切换

```cpp
enum class QualityLevel { Low, Medium, High, Ultra };

void ApplyQualityPreset(ECSContext* context, QualityLevel level) {
    // 基础元素始终启用
    context->SetElementTypeSystemsEnabled("Primitive", true);
    context->SetElementTypeSystemsEnabled("Text", true);
    
    // 高级元素按质量等级启用
    bool enable_particle = (level >= QualityLevel::Medium);
    bool enable_decal = (level >= QualityLevel::High);
    bool enable_detail = (level == QualityLevel::Ultra);
    
    context->SetElementTypeSystemsEnabled("Particle", enable_particle);
    context->SetElementTypeSystemsEnabled("Decal", enable_decal);
    context->SetElementTypeSystemsEnabled("DetailGrass", enable_detail);
}
```

### 场景3：运行时性能优化

```cpp
// 帧率下降时自动降级
void FrameRateProfiling(ECSContext* context) {
    static float frame_time = 0.016f;  // 目标60fps
    
    if (frame_time > 0.033f) {  // 掉到30fps以下
        // 禁用高成本特性
        context->SetElementTypeSystemsEnabled("Particle", false);
        context->SetElementTypeSystemsEnabled("Water", false);
        LogWarning("Performance degradation: Disabling Particle and Water");
    } else if (frame_time < 0.010f) {  // 帧率充足
        // 重新启用
        context->SetElementTypeSystemsEnabled("Particle", true);
        context->SetElementTypeSystemsEnabled("Water", true);
    }
}
```

---

## 系统组设计指南

### Primitive系统组（参考实现）
```
RenderPrimitiveCollectSystem ─┐
                              ├─ RenderPrimitiveCullSystem ─┐
                                                            ├─ RenderPrimitiveSortSystem ─┐
                                                                                         ├─ RenderPrimitiveBatchBuildSystem ─┐
                                                                                                                           ├─ RenderPrimitiveBatchFinalizeSystem ─┐
                                                                                                                                                                  ├─ RenderPrimitiveSubmitSystem
```

**六阶段模式：**
1. **Collect** - 收集符合条件的Component
2. **Cull** - 视锥体/遮挡剔除
3. **Sort** - 按深度/材质排序
4. **Batch** - 合并成批次
5. **Finalize** - 最后处理
6. **Submit** - GPU命令提交

### Text系统组（参考实现）
```
TextCollectSystem ─┐
                   ├─ TextBuildSystem ─┐
                                       ├─ TextResourceSyncSystem ─┐
                                                                  ├─ TextRenderSubmitSystem
```

**四阶段模式：**
1. **Collect** - 收集TextComponent
2. **Build** - 构建纹理/排版
3. **ResourceSync** - 同步GPU资源
4. **Submit** - 提交绘制命令

### 最小化：单System模式

```cpp
// 简单元素只需一个System
class SkySphereRenderSystem : public System {
    SkySphereRenderSystem(const std::string& name) : System(name) {
        SetExecutionOrder(ExecutionPhase::RenderPostProcess_LineRenderSystem);
        SetRenderElementType("SkySphere");  // 声明
    }
    
    void Update(float dt) override {
        // 收集+处理逻辑
    }
    
    void Render(RenderCmdBuffer* cmd, float dt) override {
        // 提交逻辑
    }
};
```

---

## 命名约定

### Element Type名称
- 使用PascalCase（驼峰）：`"SkySphere"`, `"Particle"`, `"DetailGrass"`
- 不必对应Component名，但建议保持相似

### System名称  
- 模式：`{ElementType}{Phase}System`
- 示例：
  - `SkySphereRenderSystem` （单系统）
  - `ParticleCollectSystem`, `ParticleEmitSystem`, `ParticleSimulationSystem`

### ExecutionPhase值
- 遵循现有前缀：`RenderCollect_*`, `RenderBatch_*`, `RenderDrawSubmit_*`, `RenderPostProcess_*`
- 示例：`RenderPostProcess_SkySphereRenderSystem`

---

## 诊断和调试

### 查看系统组状态

```cpp
void DebugSystemGroups(ECSContext* context) {
    const std::vector<std::string> elements = {
        "Primitive", "Text", "Line", "Billboard", "SkySphere"
    };
    
    for (const auto& elem : elements) {
        std::vector<std::shared_ptr<System>> systems;
        context->GetSystemsByElementType(elem, systems);
        
        printf("[%s] (%zu systems)\n", elem.c_str(), systems.size());
        for (const auto& sys : systems) {
            printf("  - %s (enabled=%d)\n", 
                   sys->GetName().c_str(), 
                   sys->IsEnabled());
        }
    }
}
```

### 日志输出

RenderGraph在 `CreateAdaptiveRenderGraph()` 中输出诊断日志：
```
[RenderGraph] Adaptive: Primitives=1 Text=0 Lines=1 Billboards=0
[RenderGraph] Enabling Primitive system group
[RenderGraph] Disabling Text system group (no TextComponents)
[RenderGraph] Enabling Line system group
[RenderGraph] Disabling Billboard system group (no Billboards)
```

---

## 常见模式

### 模式1：创建新的质量预设

```cpp
// 1. 在某处定义预设
enum class RenderPreset { 
    Mobile, Standard, HighEnd 
};

void ApplyPreset(ECSContext* context, RenderPreset preset) {
    switch (preset) {
        case RenderPreset::Mobile:
            context->SetElementTypeSystemsEnabled("Particle", false);
            context->SetElementTypeSystemsEnabled("Decal", false);
            break;
        case RenderPreset::Standard:
            context->SetElementTypeSystemsEnabled("Particle", true);
            context->SetElementTypeSystemsEnabled("Decal", false);
            break;
        case RenderPreset::HighEnd:
            context->SetElementTypeSystemsEnabled("Particle", true);
            context->SetElementTypeSystemsEnabled("Decal", true);
            break;
    }
    
    // 重新生成图
    context->InvalidateAdaptiveRenderGraph();
}
```

### 模式2：新元素加入时的自动检测

只需在 `GatherSceneStats()` 中添加检测，其他全自动。

### 模式3：手动控制某元素（绕过自适应）

```cpp
// 禁用自适应模式
context->SetAdaptiveRenderGraphEnabled(false);

// 手动控制
context->SetElementTypeSystemsEnabled("Terrain", false);
context->SetElementTypeSystemsEnabled("Water", true);

// 创建自定义图
RenderGraph custom_graph = RenderGraph();
// ... 添加Pass ...
context->Render(delta_time, custom_graph);
```

---

## 最佳实践

✅ **DO:**
- 在System构造函数中一定要调用 `SetRenderElementType()`
- 所有同类系统使用相同的element type
- 使用 `SetElementTypeSystemsEnabled()` 批量控制相关系统
- 在添加新系统时，更新 `GatherSceneStats()` 以自动检测

❌ **DON'T:**
- 不要在运行时改变系统的element type
- 不要用手动 `SetEnabled()` 替代 `SetElementTypeSystemsEnabled()`（除非特殊场景）
- 不要忘记在 `DefaultSystemsCP::Setup()` 中注册新系统
- 不要假设element type自动驼峰大小写

---

## 参考代码位置

- `inc/hgl/ecs/core/RenderGraph.h` - RenderGraph和SceneStats定义
- `src/ecs/core/RenderGraph.cpp` - 自适应图实现 (~280行)
- `inc/hgl/ecs/core/System.h` - System基类、ExecutionPhase定义
- `inc/hgl/ecs/core/Context.h` - GetSystemsByElementType()、SetElementTypeSystemsEnabled()
- `src/ecs/core/DefaultSystemsCP.cpp` - 系统注册示例
