# ECS 子世界（SubWorld）与离屏渲染目标

> 本文件原先为 0 字节，现补齐子世界/离屏 RT 的设计约定。
> 关联的标准化方案见 `doc/render-target-standardization-design.md`。

---

## 一、什么是子世界

子世界是指**除主世界之外，额外创建的 `ECSContext` 实例**，它拥有独立的
EntityManager、System 列表、Transform 存储与 RenderItem 存储，并绑定到一个
**离屏 RenderTarget** 而非 Swapchain。

典型用途：渲染到纹理（RTT）、后处理链、阴影图、CubeMap 烘焙、多视口。

与主世界的关系：

| 维度 | 主世界 | 子世界 |
|---|---|---|
| 创建者 | `AppFramework` | 应用代码（当前为 `example/common/OffscreenWorldRuntime.h`） |
| RenderTarget | `SwapchainRenderTarget` | 离屏 `RenderTarget` |
| 帧驱动 | `ECSContext::Render(dt)`（RenderGraph） | 手写 `RenderOnce`（见第三节） |
| 资源共享 | `GraphicsContext` 及其下所有 Manager **共享** | 同一个 `GraphicsContext` |
| 生命周期 | 随 AppFramework | 由创建方负责 |

**关键点**：子世界与主世界共享同一个 `GraphicsContext`（因而共享 TextureManager、
RenderPassManager、RenderTargetManager 等），但各自持有独立的
`RenderSystemCore` 与 `render_target`。

---

## 二、创建流程（现状，15 步）

参考实现 `example/common/OffscreenWorldRuntime.h`：

```
1.  从 owner（WorkObject）取主世界 main_world
2.  由 main_world 取 GraphicsContext gc
3.  由 gc 取 VulkanDevice device
4.  由 device 取 VulkanDevAttr dev_attr
5.  color_fmt = dev_attr->surface_format.format
6.  depth_fmt = dev_attr->physical_device->GetDepthFormat()
7.  构造 FramebufferInfo fbi(color_fmt, depth_fmt); fbi.SetExtent(w, h)
8.  rt = RenderTargetManager::CreateRTFromGraphicsContext(gc, main_world, &fbi)
       注意：此处传的是主世界 main_world，非新建的子世界
9.  world = new ECSContext(name)
10. world->SetResourceNamePrefix(prefix)   // GPU 资源分层追踪
11. world->SetRenderContext(owner->GetRenderContext())
12. 注册系统：RenderTargetSystem / RenderPrimitiveCollectSystem /
             RenderSceneUBOSystem / CameraSystem（按需再加 InputSystem）
13. rt_system->SetRenderContext + SetRenderTarget(rt)
    collect_system->SetWorld(world)
14. world->Initialize(device, rt)
15. camera_system->SetRenderContext + SetViewportInfo(rt->GetViewportInfo())
    collect_system->SetCameraInfo(camera_system->GetCameraInfo())
    render_core = make_unique<RenderSystemCore>(world); render_core->Initialize()
```

第 5~7 步的"取 surface_format + 取深度格式"是**每个离屏用例都要重复的模式**，
阶段 B 由 `RenderTargetDesc` 的默认值吸收。

---

## 三、帧驱动（现状，10 步）

参考实现 `OffscreenWorldRuntime::RenderOnce`：

```cpp
world->Tick(0.0f);
render_core->SetClearColor(clear_color);
render_core->BeginFrame();
world->SetCurrentRenderCmd(render_core->GetRenderCmd());
world->PrepareRenderPassSetup(render_core->GetSwapchainImageIndex(), 0.0f);
render_core->BeginRenderPass();
world->RenderDrawOnly(render_core->GetRenderCmd(), 0.0f);
render_core->EndFrame();
world->SetCurrentRenderCmd(nullptr);
(void)world->SubmitFrameToRenderTarget(0.0f);
```

这十步实质是主世界 `BeginManagedRenderFrame`（`src/ecs/core/Context.cpp:388`）
+ `RenderDrawOnly`（`:483`）+ `EndManagedRenderFrame`（`:434`）的**手工重排**。

与托管帧的差异：

- 跳过 `AcquireSwapchainImage`（离屏 RT 无 Swapchain 图像可获取）
- 跳过 `SyncRenderTargetViewport`（离屏 RT 尺寸固定，当前不支持 resize）
- 不经过 RenderGraph，直接调 `RenderDrawOnly`

**阶段 C 后收敛为**：`world->RenderTo(rt, clear_color, dt)`，十步消失。

---

## 四、生命周期与所有权

### RenderTarget

- 由 `RenderTargetManager::CreateRTFromGraphicsContext` 创建，
  **所有权归 `RenderTargetManager`**（阶段 A 起登记入 `registry`）。
- 销毁用 `RenderTargetManager::Destroy(rt)`，**不得手动 `delete`**，
  否则 `Release()` 会二次释放。
- `RenderTargetManager::Release()` 统一回收；析构函数亦兜底调用。
- 泄漏追踪用 `RenderTargetManager::GetAliveCount()`。

### ECSContext

- 由创建方 `new`，需显式 `world->Shutdown()` 后再 `delete`。
- 销毁顺序（**顺序敏感**）：
  1. `render_core.reset()` —— RenderSystemCore 持有 RT 指针，必须先释放
  2. `world->Shutdown()` + `delete world`
  3. `rt_manager->Destroy(rt)`

顺序颠倒会导致悬垂指针。

### 依赖注意

`RenderTargetSystem` 会在 `RenderPreBeginFrame` 相位把 RT 写入 `RenderContext`
（`src/ecs/systems/render/RenderTargetSystem.cpp:46`）。因此子世界销毁后、
主世界继续渲染前，必须确保 `RenderContext::current_render_target` 不再指向已释放的 RT。
当前由主世界下一帧的 `RenderTargetSystem::Update` 自动回写覆盖。

---

## 五、当前限制

| 限制 | 说明 | 解决阶段 |
|---|---|---|
| 不支持 resize | `IRenderTarget::OnResize` 只改 extent + 通知 UBO，不重建 texture/FBO | D |
| 样板代码多 | 15 步创建 + 10 步渲染 | C（提升为引擎内 `OffscreenWorld`） |
| 双轨驱动 | 与主世界 RenderGraph 路径平行存在 | C（`ECSContext::RenderTo`） |
| 无统一描述 | 格式/清屏色/环境散落各处 | B（`RenderTargetDesc`） |

---

## 六、目标形态（阶段 C 之后）

```cpp
graph::OffscreenWorldDesc d;
d.name   = "RTT:Offscreen";
d.width  = d.height = 512;
auto offscreen = gc->CreateOffscreenWorld(d);

Texture2D *tex = offscreen->GetColorTexture(0);
offscreen->Render(GetColor4f(COLOR::LightSkyBlue, 1.0f));
offscreen->Resize(1024, 1024);   // 阶段 D
```

`example/common/OffscreenWorldRuntime.h` 随之下线，RT 相关能力全部归入引擎。
