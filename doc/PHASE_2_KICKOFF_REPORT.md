# Phase 2 迁移开始报告

**时间：** 2026-02-14 (预期完成：2026-02-20)  
**状态：** ✅ 阶段 1 (准备) 完成  
**下一步：** 🚀 阶段 2 (兼容性层) - 立即开始

---

## 📊 当前状态

### 编译基准线

| 指标 | 值 |
|------|-----|
| 编译错误数 | 0 ✅ |
| 编译警告数 | ~10 (pwsh.exe 相关，非 C++) |
| 文件数量 | 2000+ |
| 代码行数 | ~500K |

### 备份确认

✅ 所有 4 个旧文件已备份到 `old_code_backup/`：

```
old_code_backup/
├── inc/hgl/graph/render/
│   ├── RenderFramework.h      (231 行)
│   └── SceneRenderer.h        (89 行)
└── src/SceneGraph/render/
    ├── RenderFramework.cpp    (~300 行)
    └── SceneRenderer.cpp      (~175 行)
```

### Phase 1 成果确认

| 组件 | 状态 | 位置 |
|------|------|------|
| ECSContext 增强 | ✅ 完成 | inc/hgl/ecs/core/Context.h |
| RenderSystemCore | ✅ 完成 | inc/hgl/ecs/systems/render/RenderSystemCore.h |
| WorkObject_Phase1 | ✅ 完成 | inc/hgl/WorkObject_Phase1.h |
| Phase1_Demo | ✅ 完成 | example/Phase1_Demo.cpp |

---

## 🎯 Phase 2 关键目标

### 目标 1: 创建兼容性适配器

```
新接口（IGraphicsContext）
            ↓
ECSContext 实现
            ↓
旧代码继续工作（过渡期）
            ↓
最终删除所有旧代码
```

### 目标 2: 迁移所有依赖文件

| 优先级 | 文件 | 操作 |
|--------|------|------|
| 🔴 高 | WorkObject.cpp | 迁移到 ECSContext |
| 🔴 高 | VKRenderTarget.cpp | 移除 RenderFramework 依赖 |
| 🔴 高 | SwapchainModule.cpp | 使用 ECSContext |
| 🟡 中 | RenderTargetManager.cpp | 迁移接口 |
| 🟡 中 | LineManager.cpp | 更新依赖 |
| 🟢 低 | 其他 8+ 个模块 | 清理包含 |

### 目标 3: 零编译错误

- 最后不能有任何 `error C####` 错误
- 最后不能有新增警告（除系统相关）

### 目标 4: 功能完整性

- [ ] 应用程序启动正常
- [ ] 窗口创建成功
- [ ] 基本渲染工作正常
- [ ] 无内存泄漏

---

## 📋 立即任务（今天）

### ✅ 已完成

1. ✅ 创建 `GraphicsContext.h` 新接口
   - 位置：`inc/hgl/graph/core/GraphicsContext.h`
   - 内容：160 行统一接口定义

2. ✅ 创建 `PHASE_2_EXECUTION_PLAN.md`
   - 详细的分阶段执行方案
   - 包含风险评估和时间表

3. ✅ 修复 Context.cpp 编译错误
   - 移除对 LOG_ERROR/LOG_INFO 的不必要调用
   - 编译现在通过 ✅

4. ✅ 创建代码备份
   - 四个旧文件已保存到 `old_code_backup/`
   - 可以随时参考

### ⏳ 接下来（今天/明天）

#### 任务 1: 简化 RenderFramework 为兼容层

**核心思想：** RenderFramework 继续存在，但简化为薄包装层，内部转发到 ECSContext

**步骤：**

1. 编辑 `inc/hgl/graph/render/RenderFramework.h`
  - 保留最小 API：GetECSContext(), GetDevice(), GetDefaultRenderPass()
   - 移除所有资源创建方法（由 GraphicsContext 处理）
   - 标记已弃用的方法

2. 编辑 `src/SceneGraph/render/RenderFramework.cpp`
   - 简化实现
   - 内部创建和持有 ECSContext
   - 所有访问转发到 ECSContext

**预期代码示例：**

```cpp
// NEW: RenderFramework.h (简化版)

class RenderFramework {
private:
    std::shared_ptr<ecs::ECSContext> ecs_context;
    
public:
    RenderFramework(const OSString& app_name);
    ~RenderFramework();
    
    // 新方式：返回实际的 ECS 世界
    ecs::ECSContext* GetECSContext() { return ecs_context.get(); }
    
    // 兼容方式：保留几个关键方法但内部转发
    VulkanDevice* GetDevice() { 
        return ecs_context->GetGPUDevice(); 
    }
    
    // 已弃用（已移除）
    // SceneRenderer 已从框架中删除
};
```

#### 任务 2: 创建 GraphicsModule 适配器

**核心思想：** 提供统一的图形资源创建接口

**位置：** `inc/hgl/graph/core/GraphicsModule.h`

**内容要点：**

```cpp
class IGraphicsModule {
public:
    // Material
    virtual Material* CreateMaterial(...) = 0;
    virtual Material* LoadMaterial(const AnsiString& name) = 0;
    virtual MaterialInstance* CreateMaterialInstance(...) = 0;
    
    // Buffer
    virtual DeviceBuffer* CreateUBO(...) = 0;
    virtual DeviceBuffer* CreateSSBO(...) = 0;
    
    // Texture
    virtual Texture2D* LoadTexture2D(...) = 0;
    virtual TextureCube* LoadTextureCube(...) = 0;
    
    // ... 其他方法
};
```

#### 任务 3: 在 ECSContext 中实现 GraphicsModule

**操作：** 在 `Context.h` 中添加

```cpp
class ECSContext {
private:
    IGraphicsModule* graphics_module = nullptr;
    
public:
    IGraphicsModule* GetGraphicsModule() { 
        return graphics_module; 
    }
    
    // ... 其他成员
};
```

---

## 📝 检查清单（Phase 2.1 - 兼容性层）

- [ ] 创建/编辑 GraphicsContext.h ✓ (已完成)
- [ ] 简化 RenderFramework.h
  - [ ] 保留最小 API
  - [ ] 移除资源创建方法声明
  - [ ] 添加 deprecated 标记
  
- [ ] 实现 RenderFramework.cpp
  - [ ] 创建内部 ECSContext
  - [ ] 实现转发方法
  
- [ ] 创建 GraphicsModule.h 接口
  - [ ] 定义所有资源创建方法
  
- [ ] 在 ECSContext 中实现 GraphicsModule
  - [ ] 持有 GraphicsModule* 指针
  - [ ] 提供访问方法
  
- [ ] 第一次编译
  - [ ] 零错误
  - [ ] 零新警告

---

## 🚨 关键风险和缓解

| 风险 | 概率 | 缓解策略 |
|------|------|---------|
| 隐藏依赖导致编译失败 | 🔴 高 | 保留兼容性层直到清晰 |
| 运行时崩溃 | 🟡 中 | 保守的单步测试 |
| 遗漏某个依赖 | 🟡 中 | 全面的代码搜索 |

---

## 📞 快速参考

### 旧 API（已弃用）

```cpp
auto rf = new RenderFramework("app");
rf->Init(1920, 1080);

auto mat = rf->CreateMaterial(...);      // ❌ 不可用
auto buf = rf->CreateUBO(...);           // ❌ 不可用
auto tex = rf->LoadTexture2D(...);       // ❌ 不可用
```

### 新 API（推荐）

```cpp
// Phase 1 API (已实现)
auto world = std::make_shared<ecs::ECSContext>();
world->Initialize();
world->InitializeGraphics(device, target);

// Phase 2 新增
auto graphics = world->GetGraphicsModule();
auto mat = graphics->CreateMaterial(...);   // ✅ 新方式
auto buf = graphics->CreateUBO(...);        // ✅ 新方式
auto tex = graphics->LoadTexture2D(...);    // ✅ 新方式

// RenderSystemCore 管理帧
auto render_core = std::make_unique<ecs::RenderSystemCore>(world.get());
while (running) {
    if (!render_core->BeginFrame()) continue;
    world->Render(render_core->GetRenderCmd(), dt);
    render_core->EndFrame();
}
```

---

## 📈 预期时间表

| 日期 | 任务 | 工作量 |
|------|------|--------|
| 2/14 (今天) | ✅ 准备完成；⏳ 开始兼容性层 | 2h |
| 2/14-15 | 创建适配器和接口 | 4h |
| 2/15-17 | 迁移关键文件 | 8h |
| 2/17-18 | 编译修复 | 6h |
| 2/18-19 | 测试和验证 | 6h |
| 2/19-20 | 最终化和文档 | 4h |

**总计：~30 小时工作量** (1-2 人，3-5 天)

---

## ✨ 成功指标

当以下条件都满足时，Phase 2 成功：

✅ RenderFramework 转发到 ECSContext  
✅ GraphicsModule 接口可用  
✅ 完全零编译错误  
✅ WorkObject 使用 ECSContext  
✅ RenderSystemCore 集成到主循环  
✅ 应用程序正常运行  

---

## 👉 下一步

**现在就可以开始：**

1. 打开 `PHASE_2_EXECUTION_PLAN.md` 了解详细方案
2. 开始任务 1：简化 RenderFramework.h
3. 持续编译验证（每个小改动后）

**有帮助吗？**

- 查看 `PHASE_1_QUICK_REFERENCE.md` 回顾 Phase 1 API
- 参考 `example/Phase1_Demo.cpp` 了解新代码模式
- 若有问题，参考 `PHASE_1_COMPLETION_REPORT.md` 的故障排除部分

---

**准备好了吗？让我们启动 Phase 2！** 🚀
