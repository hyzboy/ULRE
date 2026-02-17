# ULRE 渲染架构迁移改进方案

## 📋 问题分析

### 当前架构的病态关系图

```
                    ┌─────────────────────────────────────┐
                    │    RenderFramework (超级工厂)        │
                    │  - 持有所有Manager                  │
                    │  - 持有VulkanDevice                 │
                    │  - 持有ECSContext                   │
                    │  - 充当中央调度器                    │
                    └─────────────┬───────────────────────┘
                                  │ (强依赖)
                ┌─────────────────┼──────────────────┐
                │                 │                  │
                ▼                 ▼                  ▼
          ┌──────────┐      ┌──────────┐      ┌──────────────┐
          │SceneRend │      │WorkObject│      │  Managers    │
          │erer      │      │(应用层)  │      │(Material/Buf │
          │          │      │          │      │ fer/Texture) │
          └────┬─────┘      └────┬─────┘      └──────────────┘
               │                  │
      (持有ECSContext)    (通过宏委托)
               │                  │
               └────────┬─────────┘
                        │
                        ▼
                  ┌─────────────┐
                  │ ECSContext  │
                  │ & Systems   │
                  └─────────────┘
```

### 关键问题

#### 🔴 1. **RenderFramework 过度集中化（上帝对象）**
```cpp
// 问题：RenderFramework 持有太多单一责任
class RenderFramework {
    // 资源创建
    VulkanDevice* device;
    MaterialManager* material_manager;
    BufferManager* buffer_manager;
    TextureManager* tex_manager;
    
    // 渲染流程 (ECS-first)
    // RenderSystemCore drives frame lifecycle
    
    // 应用层 ECS
    ECSContext* default_ecs_context;
    
    // 窗口事件
    Window* win;
    VulkanInstance* inst;
    
    // ... 过多职责导致：
    // - 难以测试
    // - 难以扩展
    // - 难以分离关注点
};
```

#### 🔴 2. **WorkObject 通过宏隐藏依赖**
```cpp
// 问题：不透明的API，隐藏了对RenderFramework的强依赖
#define FUNC_FROM_RENDER_FRAMEWORK(return_type, func_name) \
    template<typename ...ARGS> \
    return_type func_name(ARGS...args) { \
        return render_framework ? render_framework->func_name(args...) : nullptr; \
    }

// 使用：应用代码看不到真实的依赖
WorkObject* wo = new MyWorkObject(render_framework);
wo->CreateUBO(...);  // 实际上是调用 render_framework->CreateUBO()
```

**后果：**
- 维护者不知道真实的依赖链
- 难以进行依赖注入
- 单元测试困难

#### 🟡 3. **权责不清：谁驱动渲染？**
```
当前混乱的流程:
1. WorkManager::Run() -> WorkObject::Tick()
2. WorkObject 调用业务逻辑
3. 业务逻辑调用 WorkObject::CreateXXX() 创建资源
4. 资源在 SwapchainWorkManager::Render() 中使用
5. RenderSystemCore::BeginFrame/EndFrame() 执行 ECS 渲染系统
6. ECS 系统又回过头来查询 RenderFramework 中的资源

问题: ECS 系统是被动的，不是主动驱动的
```

#### 🟡 4. **渲染驱动的职责边界**
```cpp
// RenderSystemCore 负责帧生命周期与命令提交流程
class RenderSystemCore {
    ECSContext* ecs_context;
    IRenderTarget* render_target;

    bool BeginFrame();
    void EndFrame();
};
```

**职责边界：**
- RenderSystemCore 负责帧生命周期与提交
- ECS 系统负责收集与提交渲染命令
- 管理渲染目标

#### 🟡 5. **新ECS系统与旧架构的不协调**
```cpp
// ECS系统是独立的
class RenderPrimitiveCollectSystem : public System {
    void Update(float deltaTime) {
        // 但最终渲染还是要经过 RenderFramework -> SceneRenderer
    }
};

// 导致数据流不清晰：
// ECSContext::Render() -> RenderSystems::Update()
//     -> RenderPrimitiveCollectSystem 收集数据
//     -> 数据何时、如何提交到 GPU？
```

#### 🟡 6. **资源创建的切身之痛**
```cpp
// 哪个对象负责创建资源？
RenderFramework rf;
WorkObject wo(&rf);  // 业务代码

// 选项 A: 通过 WorkObject
wo->CreateUBO(...);  // 实际上是 rf->CreateUBO()

// 选项 B: 直接通过 RenderFramework
rf->CreateUBO(...);

// 选项 C: 通过 BufferManager
rf->GetBufferManager()->CreateUBO(...);

// 问题: API 不一致，新手不知道用哪个
```

---

## 🎯 改进方案（分阶段迁移）

### 第一阶段：引入 RenderContext 模式（解耦RenderFramework）

**目标：**
- 分离资源创建与渲染驱动
- 让 ECS 成为渲染的主驱动
- 降低 WorkObject 的耦合度

#### Step 1.1: 创建 RenderContext 对象
```cpp
// 新文件：inc/hgl/graph/render/RenderContext.h
namespace hgl::graph {

/**
 * RenderContext: 渲染执行上下文
 * 职责：
 * - 提供统一的资源访问接口
 * - 抽象 RenderFramework/Managers 的细节
 * - 支持多场景多渲染目标
 */
class RenderContext {
private:
    // 资源访问接口（不直接持有，通过引用）
    VulkanDevice* device;
    TextureManager* tex_manager;
    BufferManager* buf_manager;
    MaterialManager* mat_manager;
    SamplerManager* sampler_manager;
    RenderPassManager* rp_manager;
    
    // 渲染命令缓冲区
    RenderCmdBuffer* current_cmd_buf;
    
    // 当前渲染目标
    IRenderTarget* current_render_target;

public:
    // 显式依赖注入（而不是从 RenderFramework 获取）
    RenderContext(VulkanDevice* dev,
                  TextureManager* tex_mgr,
                  BufferManager* buf_mgr,
                  MaterialManager* mat_mgr,
                  SamplerManager* sampler_mgr,
                  RenderPassManager* rp_mgr);
    
    ~RenderContext() = default;

public:
    /// ===== 资源创建接口 =====
    Material* CreateMaterial(const AnsiString& name) {
        return mat_manager->CreateMaterial(name);
    }
    
    Material* LoadMaterial(const OSString& path) {
        return mat_manager->LoadMaterial(path);
    }
    
    DeviceBuffer* CreateUBO(const AnsiString& name, VkDeviceSize size) {
        return buf_manager->CreateUBO(name, size);
    }
    
    Texture2D* LoadTexture2D(const OSString& path) {
        return tex_manager->LoadTexture2D(path);
    }
    
    Sampler* CreateSampler() {
        return sampler_manager->CreateSampler();
    }

public:
    /// ===== 渲染执行接口 =====
    void SetCurrentRenderTarget(IRenderTarget* rt) {
        current_render_target = rt;
    }
    
    IRenderTarget* GetCurrentRenderTarget() const {
        return current_render_target;
    }
    
    void BeginRenderPass(Pipeline* pipeline) {
        // 实现: 在 current_cmd_buf 上开始 RenderPass
    }
    
    void EndRenderPass() {
        // 实现: 在 current_cmd_buf 上结束 RenderPass
    }

public:
    /// ===== 低级资源访问 =====
    VulkanDevice* GetDevice() const { return device; }
    MaterialManager* GetMaterialManager() const { return mat_manager; }
    BufferManager* GetBufferManager() const { return buf_manager; }
    TextureManager* GetTextureManager() const { return tex_manager; }
    
    // 仅在必要时才暴露（逐步减少）
    [[deprecated("使用特定的访问方法而不是直接获取Manager")]]
    RenderFramework* GetRenderFramework() const { return nullptr; }
};//class RenderContext

} // namespace hgl::graph
```

#### Step 1.2: 改进 RenderFramework 提供 RenderContext
```cpp
// 修改 inc/hgl/graph/render/RenderFramework.h
class RenderFramework {
private:
    std::unique_ptr<RenderContext> default_render_context;

public:
    /// 获取渲染上下文（推荐用法）
    RenderContext* GetRenderContext() const { 
        return default_render_context.get(); 
    }
    
    // 保留旧 API 用于向后兼容（标记为 deprecated）
    [[deprecated("使用 GetRenderContext() 替代")]]
    Material* CreateMaterial(const AnsiString& name) {
        return default_render_context->CreateMaterial(name);
    }
    
    // ... 其他旧方法
};
```

#### Step 1.3: 改进 WorkObject 使用 RenderContext
```cpp
// 修改 inc/hgl/WorkObject.h
class WorkObject : public TickObject {
protected:
    graph::RenderFramework* render_framework;
    graph::RenderContext* render_context;  // 新增
    graph::SceneRenderer* scene_renderer;

public:
    // 新的访问模式（不再使用宏）
    graph::RenderContext* GetRenderContext() { 
        return render_context; 
    }
    
    // 向后兼容（逐步弃用）
    [[deprecated("使用 GetRenderContext()->CreateMaterial()")]]
    Material* CreateMaterial(const AnsiString& name) {
        return render_context ? render_context->CreateMaterial(name) : nullptr;
    }
    
    // 删除宏生成的方法
    // #undef FUNC_FROM_RENDER_FRAMEWORK
};
```

---

### 第二阶段：ECS 驱动渲染（让 ECS 成为主体）

**目标：**
- ECS 系统主动驱动渲染
- SceneRenderer 成为 ECS 的"执行器"
- 明确的数据流向

#### Step 2.1: ECS 系统注册到 RenderContext
```cpp
// 在 RenderFramework 或 ECSContext 中
class ECSContext {
private:
    RenderContext* render_context;
    
public:
    void SetRenderContext(RenderContext* ctx) {
        render_context = ctx;
    }
    
    RenderContext* GetRenderContext() const {
        return render_context;
    }
    
    // ECS 系统可通过以下方式访问渲染资源
    Material* CreateMaterial(const AnsiString& name) {
        return render_context ? render_context->CreateMaterial(name) : nullptr;
    }
};
```

#### Step 2.2: ECS 系统改为主动驱动
```cpp
// 新文件：inc/hgl/ecs/systems/render/RenderFrameSystem.h
namespace hgl::ecs {

/**
 * RenderFrameSystem: 控制整个渲染帧的系统
 * 
 * 执行顺序:
 * 1. RenderPreBeginFrame - 准备阶段（同步父/子世界）
 * 2. RenderBeginFrame    - 开始帧（获取命令缓冲区）
 * 3. RenderCollect       - 收集渲染数据（RenderPrimitiveCollectSystem）
 * 4. RenderBatch        - 批处理优化（RenderPrimitiveBatchSystem）
 * 5. RenderSubmit       - 提交绘制命令（RenderPrimitiveSubmitSystem）
 * 6. RenderCommit       - 提交缓冲区数据（RenderBufferCommitSystem）
 * 7. RenderEndFrame     - 结束帧（提交命令缓冲区）
 */
class RenderFrameSystem : public System {
private:
    RenderContext* render_context;
    IRenderTarget* render_target;
    RenderCmdBuffer* current_cmd_buf;
    
    Color4f clear_color{0,0,0,1};

public:
    RenderFrameSystem(const std::string& name = "RenderFrame");
    virtual ~RenderFrameSystem() = default;
    
    void SetRenderTarget(IRenderTarget* rt) {
        render_target = rt;
    }
    
    void SetRenderContext(RenderContext* ctx) {
        render_context = ctx;
    }
    
    void SetClearColor(const Color4f& color) {
        clear_color = color;
    }

public:
    // 帧生命周期方法
    void BeginFrame(float deltaTime);      // 获取命令缓冲区
    void Execute(float deltaTime) override; // 执行渲染
    void EndFrame(float deltaTime);        // 提交命令缓冲区
    
    RenderCmdBuffer* GetCurrentRenderCmdBuffer() const {
        return current_cmd_buf;
    }
};

} // namespace hgl::ecs
```

#### Step 2.3: SceneRenderer 转变为"执行器"
```cpp
// 修改 inc/hgl/graph/render/SceneRenderer.h
class SceneRenderer {
    // 不再是中心，而是 ECS 的一个"查询接口"
    ECSContext* ecs_context;
    RenderContext* render_context;
    
    // 不再持有 RenderFramework
    // RenderFramework* render_framework;  // 删除！

public:
    // 新的职责：提供查询接口
    Camera* GetCamera() const {
        // 从 ECS 查询
        auto sys = ecs_context->GetSystem<CameraSystem>();
        return sys ? sys->GetCamera() : nullptr;
    }
    
    // ... 其他查询方法
    
    // 渲染流程由 ECS 驱动
    // 不再有 RenderFrame() / Submit()
    // 这些现在由 RenderFrameSystem 负责
};
```

---

### 第三阶段：API 一致性与便捷性

**目标：**
- 统一的资源创建 API
- 清晰的依赖注入
- 便捷而不隐晦

#### Step 3.1: 创建 RenderAPI Facade
```cpp
// 新文件：inc/hgl/graph/render/RenderAPI.h
namespace hgl::graph {

/**
 * RenderAPI: 高级渲染 API 外观
 * 
 * 提供简洁的接口，同时保持透明的依赖注入
 */
class RenderAPI {
private:
    RenderContext* context;
    
    // 便捷缓存
    MaterialManager* mat_mgr;
    BufferManager* buf_mgr;
    TextureManager* tex_mgr;

public:
    explicit RenderAPI(RenderContext* ctx)
        : context(ctx)
        , mat_mgr(ctx->GetMaterialManager())
        , buf_mgr(ctx->GetBufferManager())
        , tex_mgr(ctx->GetTextureManager())
    {
    }

public:
    // ===== Materials =====
    Material* CreateMaterial(const AnsiString& name) const {
        return mat_mgr->CreateMaterial(name);
    }
    
    MaterialInstance* CreateMaterialInstance(Material* mtl) const {
        return mat_mgr->CreateMaterialInstance(mtl);
    }

    // ===== Buffers =====
    template<typename T>
    DeviceBuffer* CreateUBO(const AnsiString& name) const {
        return buf_mgr->CreateUBO<T>(name);
    }
    
    IndexBuffer* CreateIBO(VkDeviceSize size, IndexType type) const {
        return buf_mgr->CreateIBO(size, type);
    }

    // ===== Textures =====
    Texture2D* LoadTexture2D(const OSString& path) const {
        return tex_mgr->LoadTexture2D(path);
    }

public:
    // 显式访问底层上下文（仅在必要时）
    RenderContext* GetContext() const {
        return context;
    }
};

} // namespace hgl::graph
```

#### Step 3.2: 改进 WorkObject 使用 RenderAPI
```cpp
// 新的 WorkObject 实现
class WorkObject : public TickObject {
protected:
    graph::RenderFramework* render_framework;
    graph::RenderContext* render_context;
    std::unique_ptr<graph::RenderAPI> render_api;
    
    ecs::ECSContext* ecs_context;

public:
    WorkObject(graph::RenderFramework* rf, graph::SceneRenderer* sr = nullptr)
        : render_framework(rf)
        , render_context(rf->GetRenderContext())
        , render_api(std::make_unique<RenderAPI>(render_context))
        , ecs_context(rf->GetECSContext())
    {
    }

public:
    /// 推荐：使用 RenderAPI
    graph::RenderAPI* GetRenderAPI() { 
        return render_api.get(); 
    }
    
    /// 高级：需要ECS数据时
    ecs::ECSContext* GetECSContext() { 
        return ecs_context; 
    }
    
    /// 低级：需要精细控制时
    graph::RenderContext* GetRenderContext() { 
        return render_context; 
    }

    // 不再使用宏，删除：
    // #define FUNC_FROM_RENDER_FRAMEWORK ...
    // #undef  FUNC_FROM_RENDER_FRAMEWORK
};

// 使用示例（新代码）
class MyWorkObject : public WorkObject {
    void Init() override {
        // 方式1: 使用 RenderAPI（最简洁）
        auto mtl = GetRenderAPI()->CreateMaterial("myMaterial");
        auto ubo = GetRenderAPI()->CreateUBO<MyUBO>("myUBO");
        
        // 方式2: 使用 RenderContext（需要底层控制）
        auto tex = GetRenderContext()->LoadTexture2D("texture.png");
        
        // 方式3: 直接访问 ECS（处理业务逻辑）
        auto ecs = GetECSContext();
        ecs->GetSystem<TransformSystem>();
    }
};
```

---

### 第四阶段：逐步安全替换

**目标：**
- 旧代码继续工作
- 新代码使用新架构
- 逐步迁移，最终清理

#### Step 4.1: 兼容层（防止破坏现有代码）
```cpp
// 在 RenderFramework 中保留兼容方法（标记为 deprecated）
class RenderFramework {
public:
    [[deprecated("使用 GetRenderContext()->CreateMaterial() 替代")]]
    Material* CreateMaterial(const AnsiString& n) {
        return default_render_context->CreateMaterial(n);
    }
    
    // 提供迁移指南
    [[nodiscard]]
    static const char* GetMigrationGuide() {
        return "迁移指南:\n"
               "1. 获取 RenderContext: auto ctx = rf->GetRenderContext();\n"
               "2. 使用新 API: ctx->CreateMaterial(...);\n"
               "详见文档: https://...";
    }
};
```

#### Step 4.2: 迁移检查清单
```cpp
// 创建迁移追踪文件：MigrationChecklist.md

## WorkObject 迁移清单

### 第一批：核心类迁移
- [ ] RenderContext 创建完成
- [ ] RenderAPI Facade 完成
- [ ] ECSContext 集成 RenderContext
- [ ] RenderFrameSystem 创建

### 第二批：系统迁移
- [ ] RenderPrimitiveCollectSystem 使用新 API
- [ ] RenderPrimitiveBatchSystem 使用新 API
- [ ] RenderPrimitiveSubmitSystem 使用新 API
- [ ] RenderBufferCommitSystem 使用新 API

### 第三批：应用代码迁移
- [ ] MyWorkObject 迁移到新 API
- [ ] OtherWorkObject 迁移到新 API
- ...

### 最终：清理
- [ ] 删除 FUNC_FROM_RENDER_FRAMEWORK 宏
- [ ] 从 RenderFramework 删除过期方法
- [ ] 更新文档和教程
```

---

## 📊 迁移路线图

```
┌─────────────────────────────────────────────────────────┐
│ Phase 1: 引入 RenderContext (1-2 周)                     │
│ ✓ 创建 RenderContext 类                                 │
│ ✓ 分离资源访问接口                                      │
│ ✓ RenderFramework 返回 RenderContext                    │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│ Phase 2: ECS 驱动渲染 (2-3 周)                           │
│ ✓ 创建 RenderFrameSystem                                │
│ ✓ ECSContext 集成 RenderContext                         │
│ ✓ SceneRenderer 改为查询接口                            │
│ ✓ 修改 ECS 系统使用 RenderContext                       │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│ Phase 3: API Facade & 便捷性 (1-2 周)                    │
│ ✓ 创建 RenderAPI Facade                                 │
│ ✓ 统一资源创建 API                                      │
│ ✓ 提供清晰的依赖注入                                    │
└─────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────┐
│ Phase 4: 兼容层 & 逐步替换 (持续)                         │
│ ✓ 标记过期 API                                          │
│ ✓ 保留兼容方法                                          │
│ ✓ 迁移应用代码                                          │
│ ✓ 最终清理                                              │
└─────────────────────────────────────────────────────────┘

总时间: 6-8 周 (可并行进行)
```

---

## 🎯 关键改进效果

### Before（现状）
```
WorkObject        RenderFramework          ECSContext
    │                  │                        │
    ├──── 强依赖 ──────┼──── 强依赖 ──────────┤
    │                  │                        │
    └──── 宏隐藏 ──────┘ (不透明)              │
                       │ (RenderFramework      │
                       │  管理                  │
                       │  RenderPass)          │
                       │                        │
众多Manager ◄── 查询 ──┘                       │
(Material/Buf                                 │
fer/Texture)                                  │
```

问题：混乱、难维护、难测试

### After（改进后）
```
WorkObject
    │
    ├─── 使用 ──► RenderAPI (Facade)
    │              │
    │              └─── 委托 ──► RenderContext
    │                            │
    │                            ├─ TextureManager
    │                            ├─ BufferManager
    │                            ├─ MaterialManager
    │                            └─ SamplerManager
    │
    └─── 使用 ──► ECSContext (主驱动)
                   │
                   ├─ RenderFrameSystem
                   ├─ CameraSystem
                   ├─ TransformSystem
                   └─ ... 其他系统

RenderFramework (启动器，不是中心)
    │
    ├─ 创建 VulkanDevice
    ├─ 创建 Managers
    ├─ 创建 RenderContext
    ├─ 创建 ECSContext
    └─ 注入依赖
```

优点：
- ✅ 清晰的依赖流向
- ✅ 易于单元测试（注入 Mock）
- ✅ 易于扩展（新 Manager 只需在 RenderContext 中添加）
- ✅ 不隐晦的 API（没有宏魔法）
- ✅ ECS 成为主驱动

---

## 💼 实施建议

### 1. 代码审查清单
```
□ RenderContext 是否清晰分离了职责？
□ RenderAPI 是否提供了足够的便捷方法？
□ 所有 Manager 访问都通过 RenderContext 吗？
□ ECS 系统完全不依赖 RenderFramework 吗？
□ 是否有循环依赖？
□ 过期 API 是否都有迁移文档？
```

### 2. 测试策略
```cpp
// 单元测试示例
class RenderContextTest {
    MockVulkanDevice mock_device;
    MockMaterialManager mock_mat_mgr;
    
    RenderContext ctx(&mock_device, &mock_mat_mgr, ...);
    
    void TestCreateMaterial() {
        auto mtl = ctx.CreateMaterial("test");
        EXPECT_CALL(mock_mat_mgr, CreateMaterial("test"));
        // ... 验证
    }
};
```

### 3. 文档模板
```markdown
## 迁移指南：从 WorkObject 宏到新 API

### 旧方式
```cpp
wo->CreateUBO<MyData>("ubo");
```

### 新方式
```cpp
auto api = wo->GetRenderAPI();
api->CreateUBO<MyData>("ubo");
```

或者

```cpp
auto ctx = wo->GetRenderContext();
ctx->CreateUBO<MyData>("ubo");
```

### 何时使用哪个？
- 使用 `RenderAPI` 当你进行常规操作
- 使用 `RenderContext` 当你需要底层控制
- 使用 `ECSContext` 当你需要处理实体和系统
```

### 4. 风险评估
| 风险 | 影响 | 缓解措施 |
|-----|------|--------|
| 现有代码破坏 | 高 | 保留兼容层直到迁移完成 |
| 性能回退 | 中 | 在 Release 版本中基准测试 |
| 学习曲线 | 中 | 提供清晰的文档和示例 |
| ECS 系统复杂化 | 中 | 逐步迁移，保留测试 |

---

## 📚 参考资源

### 推荐阅读
- [Dependency Injection Pattern](https://refactoring.guru/design-patterns/dependency-injection)
- [Facade Pattern](https://refactoring.guru/design-patterns/facade)
- [Service Locator Anti-Pattern](https://martinfowler.com/articles/injection.html#UsingServiceLocator)
- [ECS Architecture Best Practices](https://medium.com/codex)

### 相关文件变更（概览）
```
新增:
  inc/hgl/graph/render/RenderContext.h
  inc/hgl/graph/render/RenderAPI.h
  inc/hgl/ecs/systems/render/RenderFrameSystem.h

修改:
  inc/hgl/graph/render/RenderFramework.h (兼容层)
  inc/hgl/graph/render/SceneRenderer.h (简化)
  inc/hgl/WorkObject.h (新 API)
  inc/hgl/ecs/core/Context.h (集成RenderContext)

逐步弃用:
  inc/hgl/WorkObject.h 中的宏
  RenderFramework 中的 Create* 方法
```

---

## ✅ 完成标志

迁移成功的标志：
1. ✅ 新应用代码完全使用 RenderAPI / RenderContext
2. ✅ ECS 系统是渲染的主驱动（不被 WorkObject 驱动）
3. ✅ RenderFramework 仅作为启动器，不是中心
4. ✅ 没有循环依赖
5. ✅ 单元测试覆盖率 > 80%
6. ✅ 编译时警告接近 0（deprecated 相关）
7. ✅ 文档和示例已更新
8. ✅ 旧 API 完全移除
