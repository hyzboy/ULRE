# Phase 2 执行方案：删除旧架构、完成 ECS 集成

**启动时间：** 2026-02-14  
**预计周期：** 2-3 周  
**目标：** 完全删除旧集中式入口，整合 RenderSystemCore

---

## 📊 现状分析

### 需要删除的文件 (4 个)

```
✂️ 旧集中式入口相关文件
```

**总计：~795 行旧代码** 

### 受影响的文件 (14+ 个)

**直接依赖文件（需要重构）：**

| 文件 | 行数 | 变更点 |
|------|------|--------|
| `src/Work/WorkObject.cpp` | 133 | 替换为 ECSContext |
| `src/Vulkan/VKRenderTarget.cpp` | ~200 | 移除旧集中式入口依赖 |
| `src/Vulkan/VKSwapchainRenderTarget.cpp` | ~180 | 移除旧集中式入口依赖 |
| `src/SceneGraph/module/SwapchainModule.cpp` | ~300 | 迁移到 ECSContext |
| `src/SceneGraph/module/RenderTargetManager.cpp` | ~150 | 迁移到 ECSContext |
| `src/SceneGraph/render/line/LineManager.cpp` | ~100 | 迁移接口 |
| `src/SceneGraph/render/line/LineRenderManager.cpp` | ~150 | 迁移接口 |
| 其他 8+ 个模块 | ~500 | 包含清理 |

---

## 🎯 分阶段实施计划

### 阶段 1: 准备和评估（第 1 天）

**任务 1.1：创建迁移分支**

```bash
git checkout -b phase-2-ecs-integration
git log --oneline -n 5  # 确认在正确的分支
```

**任务 1.2：备份旧代码**

```bash
# 在 old_code/ 目录创建备份
mkdir -p d:\ULRE\old_code_backup\inc\hgl\graph\render
mkdir -p d:\ULRE\old_code_backup\src\SceneGraph\render

cp d:\ULRE\old_code_backup\README.txt d:\ULRE\old_code_backup\
```

**任务 1.3：编译基准测试**

```bash
cd d:\ULRE\build
cmake --build . --config Debug 2>&1 | grep -E "error|warning" | head -20
# 记录当前 error/warning 计数
```

### 阶段 2: 创建兼容性层（第 1-2 天）

**任务 2.1：移除旧集中式入口**

保留最小的兼容性接口：

```cpp
// 旧集中式入口相关接口全部移除
```

**任务 2.2：创建 GraphicsModule 适配器**

这个类统一访问所有图形资源创建方法：

```cpp
// inc/hgl/graph/core/GraphicsModule.h

class IGraphicsModule {
public:
    virtual Material* CreateMaterial(...) = 0;
    virtual Buffer* CreateUBO(...) = 0;
    virtual Texture* LoadTexture(...) = 0;
    // ... 所有创建方法
};
```

**任务 2.3：在 ECSContext 中实现 IGraphicsModule**

这使现有代码通过 `ecs->GetGraphicsModule()` 获取创建接口。

### 阶段 3: 迁移关键文件（第 2-3 天）

**任务 3.1：更新 WorkObject**

```cpp
// inc/hgl/WorkObject_Phase2.h (演进版本)

class WorkObject {
private:
    std::shared_ptr<ecs::ECSContext> world;
    
public:
    WorkObject(std::shared_ptr<ecs::ECSContext> ctx) : world(ctx) {}
    
    // 直接通过 ECSContext 访问
    Material* CreateMaterial(...) {
        return world->GetGraphicsModule()->CreateMaterial(...);
    }
    
    ecs::ECSContext* GetECSContext() { return world.get(); }
};
```

**任务 3.2：更新 Render Target 系统**

```cpp
// src/Vulkan/VKRenderTarget.cpp

// 旧方式：
// IRenderTarget::IRenderTarget(legacy_entry, ...) 
//     : legacy_entry(legacy_entry) {}

// 新方式：
IRenderTarget::IRenderTarget(ecs::ECSContext* ctx, ...) 
    : ecs_context(ctx) {}

VulkanDevice* IRenderTarget::GetDevice() {
    return ecs_context->GetGPUDevice();
}
```

**任务 3.3：更新模块管理器**

| 模块 | 修改 | 优先级 |
|------|------|--------|
| SwapchainModule | 使用 ECSContext | 高 |
| RenderTargetManager | 使用 ECSContext | 高 |
| MaterialManager | 直接保存 device | 中 |
| BufferManager | 直接保存 device | 中 |
| TextureManager | 直接保存 device | 中 |

### 阶段 4: 编译和修复（第 3-4 天）

**任务 4.1：首次编译**

```bash
cd d:\ULRE
cmake --build build --config Debug 2>&1 | tee build_log.txt
# 查看错误：grep "error C" build_log.txt
```

**任务 4.2：按优先级修复错误**

| 优先级 | 错误类型 | 修复方式 |
|--------|---------|---------|
| 高 | 找不到头文件 | 更新 #include 路径 |
| 高 | 未定义的引用 | 实现必要的转发方法 |
| 中 | 类型不匹配 | 添加转换函数 |
| 低 | 警告 | 标记为 deprecated |

**任务 4.3：增量编译**

```bash
# 只编译受影响的目标
cmake --build build --config Debug --target SceneGraph 2>&1 | head -30
cmake --build build --config Debug --target Work 2>&1 | head -30
```

### 阶段 5: 功能验证（第 4-5 天）

**任务 5.1：单元测试**

```bash
# 创建最小测试程序
cd d:\ULRE\example
# 编写 Phase2_Migration_Test.cpp
```

**任务 5.2：集成测试**

- [ ] 应用程序启动成功
- [ ] 窗口创建成功
- [ ] 基本渲染工作正常
- [ ] 无内存泄漏

**任务 5.3：性能回归测试**

```
baseline (Phase 1): 60 FPS
target (Phase 2):   ≥ 59 FPS (< 2% 降低)
```

### 阶段 6: 清理和最终化（第 5-6 天）

**任务 6.1：删除兼容性层**

一旦所有文件迁移完成，删除临时兼容性代码。

**任务 6.2：代码审查和文档**

- 所有新代码都有注释
- 迁移决策都有文档
- 新 API 有示例

**任务 6.3：提交和合并**

```bash
git commit -m "Phase 2: Complete ECS migration, remove legacy entry"
git push origin phase-2-ecs-integration
# 创建 PR 请求代码审查
```

---

## 🔧 技术决策

### 决策 1: 何时删除旧文件

**方案 A（推荐）：立即删除，处理编译错误**
- ✅ 优点：更容易跟踪所有依赖
- ✅ 优点：强制迁移完成
- ❌ 缺点：短期内编译会失败

**方案 B：保留兼容层 2 周**
- ✅ 优点：逐步迁移，不中断开发
- ✅ 优点：可以并行进行其他工作
- ❌ 缺点：容易延迟完成
- ❌ 缺点：维护两套 API 的成本

**决策：** 使用 **方案 A**（立即删除）以保持清晰的目标和强制完成。

### 决策 2: 如何处理 WorkObject 子类

**当前代码模式：**

```cpp
class MyGame : public WorkObject {
public:
    MyGame(legacy_entry_t entry)
        : WorkObject(entry) {}
};
```

**新模式：**

```cpp
class MyGame : public WorkObject {
public:
    MyGame(std::shared_ptr<ecs::ECSContext> world)
        : WorkObject(world) {}
};
```

**迁移策略：**
1. 在 WorkObject_Phase2.h 中提供新接口
2. 在应用程序的 main() 中更新初始化代码
3. 更新所有 WorkObject 子类的构造函数

### 决策 3: RenderSystemCore 集成时机

**现状：** RenderSystemCore 已在 Phase 1 创建，但尚未集成到主循环中

**Phase 2 任务：**

```cpp
// main.cpp (伪代码)

int main() {
    // 创建 ECS 世界
    auto world = std::make_shared<ecs::ECSContext>("ULRE");
    
    // 初始化 GPU 资源
    auto device = CreateVulkanDevice();
    auto target = CreateRenderTarget(device);
    world->InitializeGraphics(device, target);
    
    // 创建核心渲染系统
    auto render_core = std::make_unique<ecs::RenderSystemCore>(world.get());
    render_core->Initialize();
    
    // 创建应用程序
    auto game = std::make_unique<MyGame>(world);
    game->Init();
    
    // 主循环
    while (running) {
        if (!render_core->BeginFrame()) continue;
        
        world->Tick(dt);
        game->Tick(dt);
        
        world->Render(render_core->GetRenderCmd(), dt);
        game->Render(dt);
        
        render_core->EndFrame();
    }
}
```

---

## 📈 检查清单

### 删除旧文件

- [ ] 备份旧代码到 old_code_backup/
- [ ] 删除旧集中式入口相关文件
- [ ] Git commit: "Phase 2: Remove obsolete legacy entry"

### 创建兼容性层

- [ ] 创建 WorkObject_Phase2.h （新接口）
- [ ] 创建 GraphicsModule.h (统一接口)
- [ ] 在 ECSContext 中实现 GraphicsModule

### 迁移关键文件

- [ ] WorkObject.cpp - 迁移到使用 ECSContext
- [ ] VKRenderTarget.cpp - 移除旧集中式入口依赖
- [ ] SwapchainModule.cpp - 使用 ECSContext
- [ ] 其他 10+ 个模块 - 清理包含和依赖

### 编译和测试

- [ ] 项目编译无错误
- [ ] 项目编译无新警告
- [ ] 所有新代码都有单元测试
- [ ] 集成测试通过
- [ ] 性能无回归 (< 2%)

### 文档和提交

- [ ] 更新迁移指南文档
- [ ] 更新 API 文档
- [ ] 创建代码审查 PR
- [ ] 获得 approval 并合并

---

## 🚨 风险评估

| 风险 | 概率 | 影响 | 缓解策略 |
|------|------|------|---------|
| 隐藏的依赖导致编译失败 | 高 | 高 | 全面搜索，创建兼容层 |
| 运行时崩溃（某些模块初始化失败） | 中 | 高 | 详细的编译日志，step-by-step 测试 |
| 性能回归 | 低 | 中 | 性能基准测试和分析 |
| 遗漏的文件修改 | 中 | 低 | 代码审查和测试覆盖率检查 |

---

## 📅 时间表

| 日期 | 任务 | 状态 |
|------|------|------|
| 2/14 | 阶段 1: 准备 | ✅ 进行中 |
| 2/14-15 | 阶段 2: 兼容性层 | ⏳ 即将开始 |
| 2/15-17 | 阶段 3: 文件迁移 | ⏳ 待定 |
| 2/17-18 | 阶段 4: 编译修复 | ⏳ 待定 |
| 2/18-19 | 阶段 5: 测试 | ⏳ 待定 |
| 2/19-20 | 阶段 6: 最终化 | ⏳ 待定 |

**总预计：6 天工作量（假设全职1-2人）**

---

## 📞 需要帮助？

**常见错误和解决方案：**

1. **"找不到 RenderFramework.h"**
   - ✅ 解决：删除 #include 行，使用 #include `<hgl/ecs/core/Context.h>` 代替

2. **"未定义的引用：RenderFramework::..."**
   - ✅ 解决：改为使用 `ecs->GetGraphicsModule()->...` 

3. **"SceneRenderer 是 nullptr"**
   - ✅ 解决：这已被弃用。改用 RenderSystemCore

4. **性能降低**
   - ✅ 诊断：检查是否有多余的同步或内存分配
   - ✅ 解决：使用分析器识别瓶颈

**获取帮助：**

- 查看 `PHASE_1_COMPLETION_REPORT.md` 了解新 API
- 参考 `example/Phase1_Demo.cpp` 获取实现示例
- 检查 `PHASE_1_FILE_INDEX.md` 了解接口位置

---

## ✨ 成功标志

当以下条件都满足时，Phase 2 完成：

✅ 所有 4 个旧文件都被删除  
✅ 0 个编译错误，0 个新警告  
✅ WorkObject 使用 ECSContext 初始化  
✅ RenderSystemCore 集成到主循环  
✅ 所有集成测试通过  
✅ 性能无回归  
✅ 代码审查通过并合并

---

**准备好了吗？下一步开始阶段 1！** 🚀
