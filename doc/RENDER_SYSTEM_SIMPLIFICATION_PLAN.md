# ECS 渲染系统架构简化方案

**文档版本**: 1.0  
**日期**: 2026-02-22  
**状态**: 提案阶段（待审核和补充）

---

## 1. 现状分析

### 1.1 当前架构问题

#### ExecutionPhase 膨胀
- **数量**：35+ 个细粒度枚举值
- **命名模式**：`RenderCollect_RenderPrimitiveCollectSystem`, `RenderBatch_RenderPrimitiveSortSystem` 等
- **特点**：每个系统几乎对应一个独有的 phase

```cpp
enum class ExecutionPhase {
    RenderCollect_RenderPrimitiveCollectSystem,      // Phase 17
    RenderCollect_RenderPrimitiveCullSystem,         // Phase 18
    RenderCollect_TextCollectSystem,                 // Phase 19
    RenderBatch_RenderPrimitiveSortSystem,           // Phase 20
    RenderBatch_RenderPrimitiveBatchBuildSystem,     // Phase 21
    // ... 继续
}
```

#### 管理复杂度高
- 新增任何系统都需要修改 `System.h` enum
- phase 值和系统名称存在冗余对应
- 难以理清"哪些系统属于一个逻辑组"

#### RenderGraph 时代的冗余
- RenderGraph 已实现 System 的 `enabled` flag 检查
- `RunRenderUpdatesRange()` / `RunRenderSystemsInRange()` 能按 phase 范围执行
- 细粒度 phase 的区分意义已下降

### 1.2 现有改进历程
- **Phase 1-6**：完成初步 ECS 渲染架构
- **2026-02-22**：引入 RenderGraph，支持多通道、条件执行
- **2026-02-22**：实现自适应 RenderGraph（按场景内容启用/禁用系统组）
- **2026-02-22**：开始考虑进一步简化

---

## 2. 简化方案

### 2.1 新 ExecutionPhase 定义

保留**核心阶段**，删除**细粒度系统级** phase。

```cpp
enum class ExecutionPhase
{
    // === Tick 阶段（逻辑更新） ===
    TickInput = 0,              // 输入处理
    TickPreUpdate = 1,          // 逻辑更新前
    TickPostUpdate = 2,         // 逻辑更新后
    
    // === Render 阶段（图形渲染） ===
    RenderPreBegin = 10,        // 帧前准备
                                // - SwapchainNextImageSystem
                                // - RenderTargetSystem
                                // - EnvironmentSystem
    
    RenderCollect = 11,         // 数据收集
                                // - RenderPrimitiveCollectSystem
                                // - TextCollectSystem
                                // - (未来: ParticleCollectSystem)
    
    RenderProcess = 12,         // 处理 & 批处理
                                // - RenderPrimitiveCullSystem
                                // - RenderPrimitiveSortSystem
                                // - RenderPrimitiveBatchBuildSystem
                                // - RenderPrimitiveBatchFinalizeSystem
                                // - TextBuildSystem
                                // - TextResourceSyncSystem
    
    RenderExecute = 13,         // GPU 执行（Submit）
                                // - RenderPrimitiveSubmitSystem
                                // - TextRenderSubmitSystem
                                // - LineRenderSystem
    
    RenderPostProcess = 14,     // 后处理 & 叠加层
                                // - 保留给叠加、UI、调试渲染
    
    RenderFrameEnd = 20,        // 帧末处理
                                // - SwapchainSubmitSystem
};
```

### 2.2 System 类结构变化

#### 当前
```cpp
class System {
    ExecutionPhase executionPhase;  // ~35 个值之一
    bool enabled = true;
};
```

#### 改进后
```cpp
class System {
    ExecutionPhase executionPhase;  // 只有 ~10 个值
    int executionPriority = 0;      // 同 phase 内的执行优先级（值越小越早）
    std::string systemGroup;        // "Primitive", "Text", "Line", "Quad", "Particle" 等
    bool enabled = true;            // RenderGraph 动态控制
};
```

#### 新增方法
```cpp
class System {
    void SetExecutionPriority(int priority) { executionPriority = priority; }
    int GetExecutionPriority() const { return executionPriority; }
    
    void SetSystemGroup(const std::string& group) { systemGroup = group; }
    const std::string& GetSystemGroup() const { return systemGroup; }
};
```

### 2.3 系统迁移示例

#### 迁移前
```cpp
// RenderPrimitiveCollectSystem
class RenderPrimitiveCollectSystem : public System {
    RenderPrimitiveCollectSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem);  // Phase 17
    }
};

// RenderPrimitiveSortSystem
class RenderPrimitiveSortSystem : public System {
    RenderPrimitiveSortSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch_RenderPrimitiveSortSystem);  // Phase 20
    }
};

// TextCollectSystem
class TextCollectSystem : public System {
    TextCollectSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect_TextCollectSystem);  // Phase 19
    }
};
```

#### 迁移后
```cpp
// RenderPrimitiveCollectSystem
class RenderPrimitiveCollectSystem : public System {
    RenderPrimitiveCollectSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);  // 统一 phase 10
        SetSystemGroup("Primitive");
        SetExecutionPriority(0);  // 同组内最早执行
    }
};

// RenderPrimitiveSortSystem
class RenderPrimitiveSortSystem : public System {
    RenderPrimitiveSortSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderProcess);  // 统一 phase 12
        SetSystemGroup("Primitive");
        SetExecutionPriority(2);  // 同组内中期执行
    }
};

// TextCollectSystem
class TextCollectSystem : public System {
    TextCollectSystem(const std::string& name)
        : System(name) {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);  // 统一 phase 11，但不同组
        SetSystemGroup("Text");
        SetExecutionPriority(10);  // 比 Primitive 稍晚
    }
};
```

### 2.4 RenderGraph 配合

#### Pass 定义（简化后）
```cpp
// 主场景渲染 pass
graph.Add(RenderGraph::Pass(
    ExecutionPhase::RenderCollect,
    ExecutionPhase::RenderExecute,
    nullptr,
    true,   // enabled
    true,   // runUpdate
    true,   // submitTransforms
    true    // runRender
));

// 后处理叠加 pass
graph.Add(RenderGraph::Pass(
    ExecutionPhase::RenderPostProcess,
    ExecutionPhase::RenderPostProcess,
    nullptr,
    true,
    false,
    false,
    true
));
```

#### 系统过滤逻辑保持不变
```cpp
// Context::RunRenderUpdatesRange() - 已兼容
// Context::RunRenderSystemsInRange() - 已兼容

// 系统的 enabled 标志在 loop 中检查
for (auto& entry : render_system_order) {
    if (!entry.system->IsEnabled())  // <-- 这里过滤
        continue;
    // 执行系统...
}
```

### 2.5 自适应 RenderGraph 保持不变

```cpp
SceneStats GatherSceneStats(ECSContext* context);
// 返回 { hasPrimitives, hasText, hasLines, hasBillboards, ... }

CreateAdaptiveRenderGraph(ECSContext* context);
// 根据 SceneStats 调用 system->SetEnabled(true/false)
// 按系统组禁用不需要的内容
```

---

## 3. 迁移步骤

### 3.1 Phase 1：定义新 ExecutionPhase
**文件**：`inc/hgl/ecs/core/System.h`
- 替换 ExecutionPhase enum（65 行 → 30 行左右）
- 增加 executionPriority 字段
- 增加 systemGroup 字符串字段
- 增加 Get/Set 方法

### 3.2 Phase 2：更新 System 基类
**文件**：`inc/hgl/ecs/core/System.h` + `src/ecs/core/System.cpp`
- 默认 executionPriority = 0
- 默认 systemGroup = ""（或自动从系统名称推导）

### 3.3 Phase 3：更新所有具体系统
**文件**：`src/ecs/systems/**/*.cpp` (~25+ 文件)
- 所有 `SetExecutionOrder(ExecutionPhase::Xxx_YyySyste)` → `SetExecutionOrder(ExecutionPhase::RenderCollect/Process/Execute)`
- 增加 `SetSystemGroup("Primitive"/"Text"/"Line"/"Quad")` 和 `SetExecutionPriority(N)`

**机械替换表**：
| 旧 Phase 前缀 | 新 Phase | systemGroup | 优先级范围 |
|---|---|---|---|
| RenderCollect_* | RenderCollect | "Primitive"/"Text" | 0-9 |
| RenderBatch_* | RenderProcess | "Primitive" | 10-19 |
| RenderDrawSubmit_* | RenderExecute | "Primitive"/"Text"/"Line" | 20-29 |
| RenderPostProcess_* | RenderPostProcess | "Line" | 0-9 |
| RenderPreBeginFrame_* | RenderPreBegin | "Core" | 0-9 |

### 3.4 Phase 4：更新 Context 比较逻辑
**文件**：`src/ecs/core/Context.cpp`
- `RunRenderUpdatesRange()` - 改为按 phase + systemGroup 过滤
- `RunRenderSystemsInRange()` - 改为按 phase + systemGroup 过滤

### 3.5 Phase 5：更新 RenderGraph.cpp
**文件**：`src/ecs/core/RenderGraph.cpp`
- CreateDefaultLinearGraph()、CreateAdaptiveRenderGraph() 等用新 phase 值
- 系统组启用/禁用逻辑保持不变（已通过 systemGroup 字符串管理）

### 3.6 Phase 6：测试和验证
- 编译验证
- 运行 draw_triangle、DrawText_ECS 等示例
- 验证自适应 RenderGraph 仍正确工作

---

## 4. 方案优势

| 优势 | 说明 |
|---|---|
| **代码简洁** | 35+ phase 降至 10+，代码行数减少 ~500 行 |
| **易于扩展** | 新系统只需 `SetExecutionOrder(Phase::RenderCollect)` + `SetSystemGroup("NewType")`，无需改 enum |
| **清晰的组织** | 系统组由 systemGroup 字段显式标记，便于理解逻辑结构 |
| **灵活的排序** | executionPriority 支持同 phase 内任意排序，不需要再引入新 phase |
| **与 RenderGraph 兼容** | 现有 RenderGraph 实现无需改动，phase 值调整参数即可 |
| **自适应成本降低** | 系统不再需要复杂的 phase 范围判断，直接看 enabled flag |
| **未来预留充足** | Particle、Decal、Terrain 等新系统只需补充 SceneStats + systemGroup 即可 |

---

## 5. 潜在风险与缓解

| 风险 | 影响 | 缓解方案 |
|---|---|---|
| 迁移工作量大 | ~25+ 系统文件需改 | 机械替换表 + 脚本辅助 |
| 系统执行顺序变化 | 可能引发 GPU 同步问题 | 用 executionPriority 确保相同顺序；对比性能 |
| 第三方代码兼容性 | 外部系统需跟随改 | 提供迁移文档 + 兼容层（临时） |
| systemGroup 字符串维护 | 可能拼写错误 | 考虑枚举或常量替代 |

---

## 6. 其他可选优化

### 6.1 systemGroup 枚举化
```cpp
enum class SystemGroup {
    Core,        // 基础系统
    Primitive,   // 3D 模型渲染
    Text,        // 文本渲染
    Line,        // 线框渲染
    Quad,        // 四边形（UI/Billboard）
    Particle,    // 粒子系统
    Decal,       // 贴花系统
    Terrain,     // 地形系统
};
```

### 6.2 System 依赖声明
现在注释中有依赖关系，可考虑正式化：
```cpp
class System {
    std::vector<TypeInfo> dependencies;  // 依赖的系统类型
    void AddDependency<T>() { /* 记录 T 系统的依赖 */ }
};
```

### 6.3 System 查询缓存
```cpp
class Context {
    std::map<std::string, System*> system_cache;  // 按 systemGroup 缓存
    std::vector<System*> GetSystemsByGroup(const std::string& group);
};
```

### 6.4 动态系统注册
根据 SceneStats 动态注册/卸载系统（而非仅启用/禁用）。

---

## 7. 文档维护计划

| 文档 | 位置 | 更新频率 |
|---|---|---|
| 本方案文档 | `doc/RENDER_SYSTEM_SIMPLIFICATION_PLAN.md` | 每次重大改进后 |
| ExecutionPhase 映射表 | `inc/hgl/ecs/EXECUTION_PHASE_REFERENCE.h`（新增）| 实时 |
| 系统架构图 | `doc/RENDER_PIPELINE_ARCHITECTURE.md`（新增）| 每季度审视 |
| 迁移清单 | `doc/MIGRATION_CHECKLIST.md`（新增）| 迁移期间 |

---

## 8. 审核检查清单

在开始实施前，请确认：

- [ ] ExecutionPhase 的 10+ 个新值是否满足现有和预期需求？
- [ ] executionPriority 的数值范围（0-99？0-255？）是否合理？
- [ ] systemGroup 用字符串还是枚举？
- [ ] 是否需要系统组的显式声明或自动推导？
- [ ] Context 中系统查询的性能是否受影响？
- [ ] 是否需要向后兼容层？
- [ ] 其他需要补充的改进？

---

## 9. 时间估计

| 环节 | 预估天数 |
|---|---|
| 设计评审 + 修改方案 | 0.5 |
| 实施 Phase 1-2（头文件改） | 0.5 |
| 实施 Phase 3（系统迁移） | 2 |
| 实施 Phase 4-5（Context + RenderGraph） | 1 |
| 编译、测试、调试 | 1 |
| 文档完善 | 0.5 |
| **总计** | **~5.5 天** |

---

## 10. 参考资源

- `inc/hgl/ecs/core/System.h` - ExecutionPhase enum 定义
- `src/ecs/core/DefaultSystems.cpp` - 系统注册示例
- `src/ecs/core/RenderGraph.cpp` - RenderGraph phase 使用
- `src/ecs/core/Context.cpp` - phase 范围查询实现

---

**文档编制**：AI Agent  
**最后更新**：2026-02-22  
**下一步**：等待用户补充意见和最终确认
