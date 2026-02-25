# RenderGraph Architecture - Usage Guide

## Overview

RenderGraph 是渲染管道的新配置层。它允许您定义一组顺序执行的"渲染通道"，每个通道可以：
- 指定执行的 ExecutionPhase 范围
- 关联到不同的渲染目标（RenderTarget）
- 有条件地启用/禁用
- 执行自定义的前置/后置回调

## 核心概念

### 现状（重构前）
```
ECSContext::Render(float)
  ↓
RenderPreBeginFrame() → PrepareRenderPassSetup() → RenderCollect...RenderSubmit → Submit
```
**问题**: 单硬线性流程，无法支持多 RT、条件执行或动态变更

### 新架构（RenderGraph）
```
ECSContext::Render(float, const RenderGraph&)
  ↓
For each pass in graph {
    if (pass.enabled) {
        onBeforePass() callback
        RunRenderUpdatesRange(pass.startPhase, pass.endPhase)
        onAfterPass() callback
    }
}
```
**优势**:
- ✅ 灵活的 phase 范围组合
- ✅ 支持条件执行（disable/enable passes）
- ✅ 可配置的预/后置回调
- ✅ 为多 RT 打开扩展空间

---

## API 参考

### RenderGraph Struct
```cpp
namespace hgl::ecs {
    struct RenderGraph {
        struct Pass {
            ExecutionPhase startPhase;        // 开始 phase
            ExecutionPhase endPhase;          // 结束 phase（包含）
            hgl::graph::IRenderTarget* renderTarget;  // 渲染目标（nullptr = 使用当前）
            bool enabled;                     // 是否执行
            
            std::function<void(ECSContext&, const Pass&)> onBeforePass;  // 前置回调
            std::function<void(ECSContext&, const Pass&)> onAfterPass;   // 后置回调
        };

        vector<Pass> passes;                  // 顺序执行的通道列表

        void Add(const Pass& pass);           // 添加通道
        void Clear();                         // 清空所有通道
        size_t GetPassCount() const;          // 获取总通道数
        size_t GetEnabledPassCount() const;   // 获取启用的通道数
    };

    // ECSContext 新增方法
    void Render(float deltaTime, const RenderGraph& graph);
    void Render(float deltaTime, const RenderGraph& graph, 
                const std::function<void(float)>& pre_render);

    // 工厂函数：生成默认线性流程
    RenderGraph CreateDefaultLinearGraph();
}
```

---

## 使用示例

### 1. 使用默认线性流程（向后兼容）
```cpp
ECSContext context("MainWorld");
context.Initialize();

// 原始方式（仍然有效）
context.Render(deltaTime);

// 显式使用默认图
RenderGraph defaultGraph = CreateDefaultLinearGraph();
context.Render(deltaTime, defaultGraph);
```

### 2. 自定义单通道图
```cpp
RenderGraph customGraph;
customGraph.Add(RenderGraph::Pass(
    ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
    ExecutionPhase::RenderPostProcess_LineRenderSystem,
    nullptr,  // 使用当前 RT
    true      // 启用
));
context.Render(deltaTime, customGraph);
```

### 3. 多阶段渲染（未来用于多 RT）
```cpp
RenderGraph multiPassGraph;

// Pass 1: Shadow map 渲染（未来实现）
multiPassGraph.Add(RenderGraph::Pass(
    ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
    ExecutionPhase::RenderDrawSubmit_RenderPrimitiveSubmitSystem,
    shadowMapRT,  // 切换到 shadow RT
    true
));

// Pass 2: 主画面渲染
multiPassGraph.Add(RenderGraph::Pass(
    ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
    ExecutionPhase::RenderPostProcess_LineRenderSystem,
    nullptr,  // 回到 swapchain
    true
));

context.Render(deltaTime, multiPassGraph);
```

### 4. 条件执行和回调
```cpp
RenderGraph conditionalGraph;

bool enablePostEffects = true;

// Collect + Batch + Submit
RenderGraph::Pass mainPass(
    ExecutionPhase::RenderCollect_RenderPrimitiveCollectSystem,
    ExecutionPhase::RenderPostProcess_LineRenderSystem,
    nullptr,
    true
);

// 添加前置回调：设置相机
mainPass.onBeforePass = [](ECSContext& ctx, const RenderGraph::Pass& pass) {
    LogInfo("[Custom] Before main pass: Setting up camera");
    // 可以在这里做一些额外的设置
};

// 添加后置回调：清理
mainPass.onAfterPass = [](ECSContext& ctx, const RenderGraph::Pass& pass) {
    LogInfo("[Custom] After main pass: Cleanup done");
};

conditionalGraph.Add(mainPass);

// 条件通道：仅当启用时执行后期效果
if (enablePostEffects) {
    conditionalGraph.Add(RenderGraph::Pass(
        ExecutionPhase::RenderPostProcess_LineRenderSystem,
        ExecutionPhase::RenderPostProcess_LineRenderSystem,
        nullptr,
        true
    ));
}

context.Render(deltaTime, conditionalGraph);
```

### 5. 带 pre_render 回调的图执行
```cpp
RenderGraph graph = CreateDefaultLinearGraph();

std::function<void(float)> preRender = [&](float dt) {
    LogInfo("[Custom] Global pre-render logic, deltaTime=%.3f", dt);
    // 可以在这里做全局的准备工作
};

context.Render(deltaTime, graph, preRender);
```

---

## 架构演进路线

### 当前状态（已交付）
✅ RenderGraph 结构定义和基础实现  
✅ 灵活的 phase 范围选择  
✅ Pass 级别的前置/后置回调  
✅ 向后兼容性（默认线性流程）  

### 下一步（未来工作）
⭕ **多 RT 支持完整化**
  - 实现 Pass 到不同 RT 的切换逻辑
  - RenderSystemCore 改进，支持多个命令缓冲区或 pass 管理
  - 测试 shadow mapping、reflection passes

⭕ **RenderGraph 编辑器**
  - 可视化配置渲染图
  - 保存/加载预设

⭕ **性能优化**
  - Batch pass 执行，减少 phase 遍历开销
  - 并行 pass 支持（如果数据依赖允许）

⭕ **调试工具**
  - Per-pass 性能采样
  - 可视化渲染图执行时间轴

---

## 总结

RenderGraph 恢复了架构的**灵活性**，同时保留了**清晰的 phase 编排**：

| 方面 | 之前 | 之后 |
|------|------|------|
| Phase 清晰度 | ✅ 高 | ✅ 高（保留） |
| 多 RT 支持 | ❌ 无 | ⭕ 框架就绪 |
| 条件执行 | ❌ 无 | ✅ 有 |
| 代码耦合度 | ⚠️ 中等 | ✅ 低 |
| 学习曲线 | ✅ 平缓 | ✅ 平缓 |

现在 ECSContext 不再是"超级工厂"，而是成为了**策略执行器**（policy executer），具体的渲染策略由 RenderGraph 定义。
