# SKILL: ExecutionPhase和系统执行顺序管理

## 目标
理解ExecutionPhase枚举如何定义渲染管线的执行顺序，学会为新系统选择合适的执行阶段。

## 当前ExecutionPhase结构

### 逻辑更新阶段（0-7）
```
0: TickInput_InputSystem                    // 输入收集
1: TickTransform_TransformSystem            // 变换更新
2: TickTransform_BoundingBoxUpdateSystem    // 包围盒更新
3: TickTransform_VisibilitySystem           // 可见性判断
4: TickCamera_CameraSystem                  // 相机模拟
5: TickPostCamera_FacingTransformSystem     // 面向相机变换
6: TickPostCamera_SunDirectionControlSystem // 太阳方向控制
7: TickPostCamera_TransformGizmoSystem      // 变换工具
```

### 渲染前处理（8-12）
```
8:  RenderSwapchainNextImage_*              // 交换链获取
9:  RenderPreBeginFrame_RenderTargetSystem
10: RenderPreBeginFrame_EnvironmentSystem
11: RenderBeginFrame_FrameIndexReady        // 帧号就绪回调
12: RenderBufferUpload_RenderBufferUploadSystem
13: RenderPostBeginFrame_RenderFrameBusinessSyncSystem
```

### 渲染主流程（14-17）
```
📊 收集阶段 (14)
14: RenderCollect_RenderPrimitiveCollectSystem

📊 批处理阶段 (15-16)
// 注：Batch/Cull/Finalize 系统已整合进 PipelineGroup
15: RenderBatch_[PrimitiveRenderPipelineGroup内部]
16: RenderBatch_[TextRenderPipelineGroup内部]

📊 提交阶段 (17-18)
17: RenderDrawSubmit_[PrimitiveRenderPipelineGroup.Draw]
18: RenderDrawSubmit_[TextRenderPipelineGroup.Draw]

📊 后处理 (19)
19: RenderPostProcess_[LineRenderPipelineGroup.Draw]
```

### 帧提交阶段（20+）
```
20: RenderStat_LineStatsSystem
21: RenderSubmit_SwapchainSubmitSystem      // 帧提交
```
```

---

## 执行流程图

```
主线程:
┌─────────────────────────────────────────────────────────────┐
│ Tick Phase (逻辑更新)                                        │
│ 0-7: Input → Transform → Camera → Visibility               │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Pre-Render Phase (准备)                                      │
│ 8-16: SwapchainAcquire → RenderTarget → Buffer*             │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Render Main Phase (主渲染)                                   │
│ 17-27:                                                      │
│   Collect → Cull → Sort → Batch → Finalize → Submit        │
│   (从Component收集数据)  (处理/优化)    (提交GPU命令)      │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│ Frame Submit Phase (提交)                                    │
│ 28+: Swapchain Present                                       │
└─────────────────────────────────────────────────────────────┘
```

---

## 为新系统选择ExecutionPhase

### 决策表

| 需求 | 推荐阶段 | 解释 |
|------|---------|------|
| 获取输入 | `TickInput_*` | 最早，供后续系统使用 |
| 更新对象变换 | `TickTransform_*` | 在相机之前，用于坐标同步 |
| 依赖相机矩阵 | `TickCamera_*` 之后 | 确保相机已更新 |
| 渲染前资源准备 | `RenderPreBeginFrame_*` | 在主渲染前准备GPU资源 |
| 数据收集（Collect） | `RenderCollect_*` | 从Component收集需要渲染的对象 |
| 数据处理（排序/裁剪） | `RenderBatch_*` | 处理已收集数据 |
| GPU命令提交 | `RenderDrawSubmit_*` | 向指令缓冲写入绘制调用 |
| 渲染后处理 | `RenderPostProcess_*` | 特效/叠加层，如线条/UI |
| 最后收尾 | `RenderSubmit_*` | 交换缓冲/present |

---

## 添加新系统的ExecutionPhase

### ⚠️ 新架构：不再需要新增 ExecutionPhase 枚举值

**当前架构（推荐）：**
- System.h 只定义 **12 个通用阶段**（TickInput, TickTransform, ..., RenderCollect, RenderBatch, RenderDrawSubmit, RenderSubmit 等）
- 每个系统通过 `SetExecutionPhase(ExecutionPhase::Xxx)` 指定在哪个阶段执行
- **依赖关系通过 `AddDependency<>()` 解决，不再用自定义 Phase 值排序**

### 场景1：简单后处理元素（如SkySphere）

```cpp
class SkySphereRenderSystem : public System {
    SkySphereRenderSystem(const std::string& name) : System(name) {
        SetExecutionPhase(ExecutionPhase::RenderPostProcess);  // 通用阶段
        SetRenderElementType("SkySphere");
    }
};
```

### 场景2：多阶段元素（如Particle系统）

不需要在 System.h 添加新 Phase，直接使用通用阶段 + 依赖关系：

```cpp
// Collect 系统
class ParticleCollectSystem : public System {
    ParticleCollectSystem() : System("ParticleCollectSystem") {
        SetExecutionPhase(ExecutionPhase::RenderCollect);  // 通用阶段
        SetRenderElementType("Particle");
    }
};

// Sort 系统
class ParticleSortSystem : public System {
    ParticleSortSystem() : System("ParticleSortSystem") {
        SetExecutionPhase(ExecutionPhase::RenderBatch);    // 通用阶段
        SetRenderElementType("Particle");
        AddDependency<ParticleCollectSystem>();             // 依赖关系
    }
};

// Submit 系统
class ParticleSubmitSystem : public System {
    ParticleSubmitSystem() : System("ParticleSubmitSystem") {
        SetExecutionPhase(ExecutionPhase::RenderDrawSubmit);  // 通用阶段
        SetRenderElementType("Particle");
        AddDependency<ParticleSortSystem>();
    }
};
```

### 场景3：渲染前处理（如自定义资源准备）

```cpp
class MyResourcePrepareSystem : public System {
    MyResourcePrepareSystem() : System("MyResourcePrepareSystem") {
        SetExecutionPhase(ExecutionPhase::RenderResourceSetup);  // 通用阶段
        SetRenderElementType("MyElement");
    }
};
```

---

## ExecutionPhase设计原则

### 原则1：依赖关系优先
使用 `AddDependency<>()` 确保逻辑顺序，即使ExecutionPhase不同：

```cpp
class A : public System {
    A() { 
        SetExecutionPhase(ExecutionPhase::RenderCollect);  // 通用阶段
    }
};

class B : public System {
    B() { 
        SetExecutionPhase(ExecutionPhase::RenderBatch);  // 不同阶段
        AddDependency<A>();  // 但保证在A之后执行
    }
};
```

### 原则2：同阶段系统无序
相同ExecutionPhase的系统执行顺序**不确定**，除非有依赖关系：

```cpp
// RenderCollect阶段可能有多个系统：
RenderPrimitiveCollectSystem   // phase: RenderCollect
MyParticleCollectSystem        // phase: RenderCollect

// ❌ 错误做法：依赖两者的默认执行顺序
// ✅ 正确做法：需要顺序则明确 AddDependency<RenderPrimitiveCollectSystem>()
```

### 原则3：阶段区间的含义

- **TickInput-TickPostCamera (Tick阶段)** ：逻辑更新，组件状态变化
- **RenderBeginFrame** ：帧开始，交换链获取
- **RenderCollect** ：扫描可见Component（如RenderPrimitiveCollectSystem）
- **RenderBatch** ：PipelineGroup内部处理（Cull/Sort/Build/Finalize）
- **RenderBufferUpload** ：GPU上传（RenderBufferUploadSystem）
- **RenderFrameSync** ：帧UBO/描述符同步
- **RenderDrawSubmit** ：PipelineGroup提交GPU绘制命令
- **RenderPostProcess** ：后处理/叠加（如LineRenderPipelineGroup）
- **RenderDebug** ：调试覆盖层（预留）
- **RenderStat** ：统计系统（如LineStatsSystem）
- **RenderSubmit** ：帧级提交（SwapchainSubmitSystem）

---

## 向ExecutionPhase添加新值

### 步骤

1. **编辑 `inc/hgl/ecs/core/System.h`**

   在合适位置添加新ExectuionPhase：
   
   ```cpp
   enum class ExecutionPhase {
       // ... 现有值到27 ...
       RenderPostProcess_LineRenderSystem,
       
       // 新增 (下一个available number = 28)
       RenderPostProcess_SkySphereRenderSystem,  // 28
       RenderPostProcess_ParticleRenderSystem,   // 29
       
       RenderSubmit_SwapchainSubmitSystem        // 30 (或重新编号)
   };
   ```

2. **更新Context中的range检查**（如有）

   检查 `src/ecs/core/Context.cpp` 中是否有硬编码的phase范围：
   
   ```cpp
   // 查找这样的代码：
   // const bool phase_is_render = effective_phase >= 9 && effective_phase <= 30;
   // 更新范围参数
   ```

3. **更新RenderGraph中的Pass范围**（如有）

   检查 `src/ecs/core/RenderGraph.cpp`：
   
   ```cpp
   // 如果有硬编码的pass范围，需要调整
   graph.Add(RenderGraph::Pass(
       ExecutionPhase::RenderCollect_*,
       ExecutionPhase::RenderPostProcess_*,  // 更新end phase
       nullptr, true, true, true, true
   ));
   ```

4. **编译和测试**

   ```bash
   cmake --build . --config Release --target ULRE.ECS
   ```

---

## 常见执行顺序问题

### 问题1：系统没有执行

**原因：**
- System未注册到Context
- System::IsEnabled() 返回false
- System::Initialize() 未被调用

**诊断：**
```cpp
// 检查系统是否存在
auto sys = context->GetSystem<MySystem>();
if (sys) {
    printf("System found, enabled=%d, initialized=%d\n", 
           sys->IsEnabled(), sys->IsInitialized());
}
```

### 问题2：系统执行顺序不对

**原因：**
- 依赖关系未正确声明
- 另一个系统有相同的ExecutionPhase但依赖关系弱

**诊断：**
```cpp
// 打印系统执行序列
for (const auto& sys : context->GetAllRenderSystems()) {
    printf("Phase %d: %s (enabled=%d)\n", 
           sys->GetExecutionPhase(), 
           sys->GetName().c_str(), 
           sys->IsEnabled());
}
```

### 问题3：某系统必须在另一个系统之前/之后执行

**解决：**

定义明确的依赖关系：
```cpp
class MySystemB : public System {
    MySystemB() : System("MySystemB") {
        // ...
        AddDependency<MySystemA>();  // B依赖A，A先执行
    }
};
```

或调整ExecutionPhase（不推荐，要和其他系统协调）。

---

## 最佳实践

✅ **DO:**
- 使用 `AddDependency<>()` 而非依赖ExecutionPhase数字
- 为新系统创建专用的ExecutionPhase枚举值
- 在添加新系统前检查相似系统的phase选择
- 记录系统的执行前置条件

❌ **DON'T:**
- 不要假设ExecutionPhase值的连续性
- 不要在多个系统间重复相同的phase（除非有意设计）
- 不要跨越太多phase（如从8跳到27）而不声明依赖
- 不要忘记在 `DefaultSystemsCP::Setup()` 中注册新系统

---

## 参考源码

- `inc/hgl/ecs/core/System.h` 第45-105行 - ExecutionPhase完整定义
- `src/ecs/core/Context.cpp` 第799行 - AddOrUpdateSystem()
- `src/ecs/core/RenderGraph.cpp` 第40-80行 - RenderGraph::Pass创建

---

## 简化未来的规划

**未来改进方向（5.5天计划第3阶段）:**

**✅ 当前架构已简化：**

系统已从 35+ 个细粒度 ExecutionPhase 值简化为 **12 个通用阶段**，依赖关系通过 `AddDependency<>()` 管理：

```cpp
// 当前架构（已实现）
class MySystem : public System {
    MySystem() : System() {
        SetExecutionPhase(ExecutionPhase::RenderCollect);  // 通用阶段
        SetRenderElementType("MyElement");
        AddDependency<OtherSystem>();  // 显式依赖关系
    }
};
```

**好处：**
- 简化了 System.h（只需维护 12 个枚举值）
- 依赖关系更明确（不再依赖 Phase 数值排序）
- 更容易添加新的渲染元素类型
