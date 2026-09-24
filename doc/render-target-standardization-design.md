# RenderTarget 标准化设计

> 目标：把 RenderTarget 从"能用但要背隐式契约"变成"声明式资源 + 统一入口"，
> 让未来所有需要 RT 的工作（离屏、后处理、阴影图、CubeMap、多视口）走同一套路。
>
> 分析入口：`example/Basic/RenderToTexture.cpp`
> 现状普查范围：`example/`、`inc/`、`src/`（不含 `build/`）

## 实施进度

| 阶段 | 状态 | 说明 |
|---|---|---|
| A 收敛 | **已完成** | 见下方"阶段 A"各项，均带 [x] |
| B 描述子与工厂 | **已完成** | 见下方"阶段 B"各项，均带 [x] |
| C OffscreenWorld 入引擎 | **已完成** | 见下方"阶段 C"各项，均带 [x] |
| D resize 与 RenderGraph 集成 | **部分完成** | resize 与 GUI 清理已完成；RenderGraph 跨 RT pass 链拆分见下方说明 |
| E depth-only（shadow map） | **已完成** | 见下方"阶段 E"，`example/Basic/ShadowMap.cpp` 落地验证 |

**本文档主体计划至此全部落地**。阶段 E 之后的后续修复（2026-09-21，
不再回写正文，详见 git log）：depth-only 管线剥离 fragment stage
（a379c712f）；RenderTo 同步 RenderTargetSystem + 管线缓存按 RenderPass
持有（c778f1fb2）；RenderTargetData::Clear 真正销毁 queue/cmd_buf
（337968861）；MaterialSSBO → GlobalSSBO 改名（e9f5ec8bd）。

离屏/子世界的现行形态：引擎内 `graph::OffscreenWorld`
（`inc/hgl/graph/module/OffscreenWorld.h`）——RT + ECSContext + 系统一行
创建，`Render()`/`Resize()` 驱动；原 `doc/ecs/ecs_sub_world.md` 所述的
ContextRole 门控从未启用、已随 ECS 清理删除。

---

## 一、现状：三层体系如何串起来

### 1.1 应用层 WorkObject

`RunFramework<WO>()`（`inc/hgl/framework/WorkManager.h:77`）是唯一入口，做四件事：

1. `AppFramework app(title)` → `app.Init(w,h,argc,argv)`
   内部建 `GraphicsContext`、`SwapchainModule`、`SwapchainRenderTarget`。
2. 取 `app.GetECSContext()`，**包成 non-owning shared_ptr**（`WorkManager.h:98`，deleter 是空 lambda）。
3. `new WO()` → `wo->_InitializeWithECSContext_INTERNAL_DO_NOT_CALL(world)` → `wo->Init()`。
4. `wm.Run(wo)` 进入主循环。

`WorkManager::Run`（`src/Work/WorkManager.cpp:86`）：`app.Tick()` → `Tick(wo)` → `Render(wo)` → `win->Update()`。

`WorkManager::Render`（`WorkManager.cpp:34`）的关键一行：

```cpp
wo->GetECSContext()->Render(static_cast<float>(delta_time),
                            [wo](float dt){ wo->OnRenderPass(static_cast<double>(dt)); });
```

**`WorkObject::OnRenderPass` 是渲染帧内的录制回调，不是绘制入口，也不是逻辑更新处。**
它在 `BeginRenderPass`（动态渲染 pass 已开）之后、ECS 系统绘制之前执行——
合同是"current_render_cmd 有效，只准录制绘制命令"；改场景状态（transform/材质动画等）
属于 `Tick`（TransformSystem 在该回调之后才提交变换，两处改同帧等价，Tick 语义正确）。
范本：`example/Basic/SimpleMeshTriangle.cpp`。（旧名 `Render` 保留为 deprecated 别名；
本文档早期版本误写为"BeginRenderPass 之前执行"，已修正。）

`WorkObject` 提供的能力全部是转发：`GetECSContext` / `GetRenderContext` / `GetGraphicsContext`
/ `GetDevice` / `GetManager<T>()` / `GetCamera` / `GetViewportInfo` / `SetClearColor`。
它本身**不持有 RT**，只持有 world 与 render_context。

### 1.2 世界层 ECSContext

`inc/hgl/ecs/core/Context.h:68`。持有：entity_manager、tick_systems / render_systems 双列表、
component_registry、transform_storage、`gpu_device`、`render_target`、`render_core`、
`graphics_context` / `render_context`、`resource_name_prefix`。

初始化唯一入口 `Initialize(device, target)`（`Context.h:230`，W3 合并产物）。

帧相位链（真源 `inc/hgl/ecs/core/System.h:22-48` 的 `enum class ExecutionPhase`；驱动程序见 `Context.cpp:620-638`、`:719-732`、`:425-443`、`:733-738`）：

```
RenderPreBeginFrame → RenderSwapchainNextImage
→ PrepareRenderPassSetup(帧索引)：RenderCollect → RenderBatch
                                    → RenderBufferCommit → RenderBufferUpload → RenderFrameSync
→ [BeginRenderPass] → RenderDrawOnly：重放 RenderCollect … RenderStat 区段
                       （只发绘制命令；Update 类相位已在 BeginRenderPass 前跑完）
→ RenderSubmit
```

> 校对注（2026-09）：旧文写的 `RenderResourceSetup` / `RenderMaterialBind` / `RenderBeginFrame`
> 三个相位名在代码里 0 命中，已按 `ExecutionPhase` 枚举替换。

两条对外驱动方式：

- **托管帧**：`BeginManagedRenderFrame`（`Context.cpp:388`）→
  `AcquireSwapchainImage` → `RenderPreBeginFrame` → `SyncRenderTargetViewport` →
  `render_core->BeginFrame()` → `PrepareRenderPassSetup` → `render_core->BeginRenderPass()`
- **图驱动**：`Render(dt)` → 选 adaptive/linear RenderGraph → `ExecuteRenderGraphPasses`

`RenderDrawOnly(cmd, dt)`（`Context.cpp:483`）是"只录绘制"的兼容入口，被离屏路径依赖。

### 1.3 目标层 IRenderTarget

`inc/hgl/vk/VKRenderTarget.h:37`。基类持有 `ecs_context`、`extent`、`env_profile`，
接口分四组：附件（Framebuffer / RenderPass / Color / Depth / RenderingAttachment）、
命令缓冲（Queue / CmdBuffer / BeginRender / EndRender）、同步提交（Submit / WaitQueue / WaitFence）、
元信息（Extent / ViewportInfo / ColorCount / hasDepth）。

两个实现：

| 实现 | 文件 | 类名 | 创建者 |
|---|---|---|---|
| 离屏 | `VKRenderTargetSingle.h` | `RenderTarget` | `RenderTargetManager::CreateRTFromGraphicsContext` |
| 主窗口 | `VKRenderTargetSwapchain.h` | `SwapchainRenderTarget` | `SwapchainModule::CreateSwapchainRenderTarget` |

`SwapchainRenderTarget` 额外有 `NextFrame()`，不在基类接口里。

### 1.4 离屏现状：OffscreenWorldRuntime

`example/common/OffscreenWorldRuntime.h` 是全仓**唯一**的离屏封装（163 行），
被 `RenderToTexture.cpp` 唯一使用。它的 `Init` 做了 15 步：

```
取 main_world → gc → device → dev_attr
color_fmt = dev_attr->surface_format.format
depth_fmt = physical_device->GetDepthFormat()
FramebufferInfo fbi(color_fmt, depth_fmt); fbi.SetExtent(w,h)
rt_ = RenderTargetManager::CreateRTFromGraphicsContext(gc, main_world, &fbi)   // 注意传的是主世界
world_ = new ECSContext(name); SetResourceNamePrefix; SetRenderContext
注册 RenderTargetSystem / RenderPrimitiveCollectSystem / RenderSceneUBOSystem / CameraSystem
rt_system_->SetRenderContext + SetRenderTarget(rt_)
collect_system_->SetWorld(world_)
world_->Initialize(device, rt_)
camera_system_->SetRenderContext + SetViewportInfo(rt_->GetViewportInfo())
collect_system_->SetCameraInfo(...)
render_core_ = make_unique<RenderSystemCore>(world_); Initialize()
```

`RenderOnce` 又手写 10 步（`OffscreenWorldRuntime.h:126`）：

```cpp
world_->Tick(0.0f);
render_core_->SetClearColor(clear_color);
render_core_->BeginFrame();
world_->SetCurrentRenderCmd(render_core_->GetRenderCmd());
world_->PrepareRenderPassSetup(render_core_->GetSwapchainImageIndex(), 0.0f);
render_core_->BeginRenderPass();
world_->RenderDrawOnly(render_core_->GetRenderCmd(), 0.0f);
render_core_->EndFrame();
world_->SetCurrentRenderCmd(nullptr);
(void)world_->SubmitFrameToRenderTarget(0.0f);
```

这段是 `BeginManagedRenderFrame` + `RenderDrawOnly` + `EndManagedRenderFrame` 的**手工重排**，
与 `Context.cpp:388/434` 的托管帧逻辑平行存在、各自演化。

---

## 二、问题清单

| # | 问题 | 证据 | 影响 |
|---|---|---|---|
| P1 | **API 面冗余**：4 个 Create 重载、5 个 getter、4 个 setter | `RenderTargetManager.h:36-43`；`Context.h:349`、`RenderTargetSystem.h:35`、`RenderContext.h:68` | 新人不知道该用哪个 |
| P2 | **死代码**：`CreateRT` 两个重载全仓零调用 | `RenderTargetManager.cpp:35` / `:95` | 误导 + 维护负担 |
| P3 | **生命周期裸指针**：谁 new 谁 delete，Manager 不持有列表 | `RenderTargetManager.h:45` `Release()` 空实现；`OffscreenWorldRuntime.h:52` | 无统一回收、无泄漏追踪 |
| P4 | **离屏 RT 不支持 resize**：`OnResize` 只改 extent + 通知 UBO，不重建 texture/FBO | `VKRenderTarget.cpp:39-49`；`OffscreenWorldRuntime` 无 Resize 方法 | 窗口自适应无从谈起 |
| P5 | **帧驱动双轨**：主路径 RenderGraph vs 离屏手写 RenderOnce | `Context.cpp:388` vs `OffscreenWorldRuntime.h:126` | 同一语义两份实现，必然漂移 |
| P6 | **样板 100+ 行**：只为开一个 RT | `OffscreenWorldRuntime.h` | 每个用例复制粘贴 |
| P7 | **clear_color 三处存储**：`WorkObject::clear_color`、`ECSContext::clear_color`、`RenderSystemCore::clear_color` | `WorkObject.h:53`、`Context.h:174`、`RenderSystemCore.h:72` | 语义分散，改一处不够 |
| P8 | **命名不一致**：`IRenderTarget` / `RenderTarget` / `SwapchainRenderTarget`；文件 `VKRenderTargetSingle.h` ↔ 类 `RenderTarget`；`FramebufferInfo` / `FBOInfo` / `SwapchainRenderbufferInfo` 三套 | `VKRenderbufferInfo.h:176/189/224` | 读代码要猜 |
| P9 | **子世界文档缺失** | `doc/ecs/ecs_sub_world.md` 为 0 字节 | 离屏约定无文字依据（该文档曾补写，已随 ContextRole 门控删除一并移除） |

---

## 三、标准化设计

核心思想一句话：**RenderTarget 成为一种声明式资源，渲染变成"把一个 world 提交到某个 RT"的统一动作。**

### 3.1 统一描述子 `RenderTargetDesc`

```cpp
namespace hgl::graph {

enum class RenderTargetKind { Offscreen, Swapchain };

struct RenderTargetDesc
{
    // ---- 必填 ----
    RenderTargetKind kind = RenderTargetKind::Offscreen;
    std::string      name;                 // 资源追踪前缀，参与 Texture/FBO 命名
    uint32_t         width  = 0;
    uint32_t         height = 0;

    // ---- 附件 ----
    std::vector<VkFormat> color_formats;   // 空 = 使用默认 surface format 单附件
    VkFormat         depth_format = VK_FORMAT_UNDEFINED;  // UNDEFINED = 取设备默认深度格式
    bool             has_depth    = true;
    uint32_t         samples      = 1;     // >1 时启用 MSAA 并自动 resolve

    // ---- 行为 ----
    bool             resizable    = true;  // 是否允许 OnResize 时按 desc 重建
    uint32_t         fence_count  = 1;

    // ---- 渲染参数（原散落三处，收归 RT）----
    Color4f          clear_color{0,0,0,1};
    EnvProfileID     env_profile = kEnvProfileDefault;

    // ---- 工厂预设 ----
    static RenderTargetDesc OffscreenColorDepth(uint32_t w, uint32_t h, std::string name = {});
    static RenderTargetDesc OffscreenColorOnly (uint32_t w, uint32_t h, std::string name = {});
};
}
```

`env_profile` 与 `clear_color` 收归 RT，消除 P7。

### 3.2 统一入口：一个 Create 签名 + RAII 句柄

```cpp
using RenderTargetHandle = std::unique_ptr<IRenderTarget, RenderTargetDeleter>;

class RenderTargetManager
{
    // 唯一创建入口（替换现有 4 个重载）
    RenderTargetHandle Create(const RenderTargetDesc &desc);

    // 统一生命周期：Manager 持有注册表
    void   Release(RenderTargetHandle);          // 或依赖句柄自动回收
    void   OnResize(const VkExtent2D &) override;// 遍历 resizable 的 RT 重建
    size_t GetAliveCount() const;                // 泄漏追踪

private:
    struct Entry { RenderTargetDesc desc; IRenderTarget *rt; };
    std::vector<Entry> registry;
};
```

要点：

- 删除 `CreateRT` 两个死重载（P2）。
- `CreateRTFromGraphicsContext` 收敛为 `Create` 的内部实现，不再对外。
- Swapchain RT 也走 `Create`（`kind = Swapchain`），由 `SwapchainModule` 提供 desc，
  使两条路径共用同一份注册/回收/resize 逻辑（P3、P4）。
- 重建时按 `desc` 重新走一遍 Create，纹理与 FBO 全部重建；可选 `preserve_content` 做 blit 保留内容。

### 3.3 统一获取：单一权威 getter

保留 `ECSContext::GetRenderTarget()` 为**唯一权威**。

- `RenderContext::SetCurrentRenderTarget` 不再由应用手动调用，
  只由 `RenderTargetSystem::Update`（`RenderTargetSystem.cpp:46`）在
  `RenderPreBeginFrame` 相位自动同步。
- 其余 getter（`AppFramework::GetSwapchainRenderTarget`、`SwapchainModule::GetRenderTarget`）
  标注为"框架内部接线用"，应用代码不直接使用。

### 3.4 统一帧驱动：把 RenderOnce 折叠进 ECSContext

新增与 `Render` 对等的一等 API：

```cpp
// 把本 world 的一帧渲染到指定 RT（离屏/子世界的主入口）
bool ECSContext::RenderTo(IRenderTarget *rt, const Color4f &clear, float dt = 0.0f);
```

实现内部复用既有的 `BeginManagedRenderFrame` / `RenderDrawOnly` / `EndManagedRenderFrame`
三段（`Context.cpp:388/483/434`），**不新增第三段逻辑**（P5）。
`OffscreenWorldRuntime::RenderOnce` 退化为一行转发，随后整个文件删除。

**该正式化已落地（2026-09）**：`RenderPassRequest`（`inc/hgl/ecs/core/RenderPassRequest.h:31`）+
`ECSContext::RenderTo(const RenderPassRequest &)`（`src/ecs/core/Context.cpp:445`）已是离屏/子 pass 一等入口，
内部复用 `BeginManagedRenderFrame(…, need_swapchain_acquire=false, &RenderPassOptions)` + `RenderDrawOnly`
+ `EndManagedRenderFrame`，字段含 `target / camera / clear_color / use_target_clear / load_depth /
use_scissor / scissor / clear_scissor_depth / mobility_filter / cull_mode_override`；`RenderPassOptions` 在
`inc/hgl/vk/VKCommandBuffer.h:116`。实际使用者：`example/Basic/ShadowMap.cpp:1097`、
`example/Basic/CascadeShadowMap.cpp:683,704`。剩余缺口是 `RenderGraph::Pass::renderTarget`
跨 RT pass 链（执行器目前只输出 `LogWarning`，见 `src/ecs/core/RenderGraph.cpp:110-118`）。

**pass 级剔除覆盖（`cull_mode_override`）**：`-1` 表示自动（沿用材质配置），其余值为
`VkCullModeFlags`。自动规则：**depth-only 目标**（零颜色附件——本引擎里只有 shadow map）
默认渲染模型**背面**（`VK_CULL_MODE_FRONT_BIT`），即阴影贴图记录背向光源的表面深度，
以减轻自阴影 acne / peter-panning；其余 pass 沿用材质配置。材质显式声明双面
（`MaterialPipelineConfig::cull_mode == VK_CULL_MODE_NONE`）时不受覆盖改写。
绕序前提：主相机透视投影与光源正交投影同为 RH + 负 Y 分量（`m11 < 0`），二者绕序不反转，
故剔除正面即剔除几何正面。

### 3.5 OffscreenWorld 提升为引擎设施

把 `example/common/OffscreenWorldRuntime.h` 从示例目录提升进引擎：

```
inc/hgl/graph/module/OffscreenWorld.h
src/SceneGraph/module/OffscreenWorld.cpp
```

对外只需三行：

```cpp
graph::OffscreenWorldDesc d;
d.name = "RTT:Offscreen"; d.width = d.height = 512;
auto offscreen = gc->CreateOffscreenWorld(d);   // RT + ECSContext + 系统 + RenderCore 全包

Texture2D *tex = offscreen->GetColorTexture(0);
offscreen->Render(GetColor4f(COLOR::LightSkyBlue, 1.0f));
offscreen->Resize(1024, 1024);                  // 阶段 D 后可用
```

`RenderToTexture.cpp` 里 86~355 行的 `OffscreenPass` 类随之消失（P6）。

### 3.6 命名规范化

| 现状 | 目标 | 说明 |
|---|---|---|
| `RenderTarget`（离屏实现） | `OffscreenRenderTarget` | 与 `SwapchainRenderTarget` 对称 |
| `VKRenderTargetSingle.h` | `VKOffscreenRenderTarget.h` | 文件名与类名对齐 |
| `IRenderTarget` | 保留 | 抽象基类，`I` 前缀约定已有 |
| `FBOInfo` | 保留为 deprecated 别名 | 新代码一律 `FramebufferInfo` |
| `SwapchainRenderbufferInfo` | 移入 `SwapchainModule` 内部 | 非公共概念 |
| `RenderTargetManager::CreateRT*` | `Create` | 单入口 |

---

## 四、实施路线

### 阶段 A — 收敛（无破坏，可独立合入）

- [x] 1. 删除 `RenderTargetManager::CreateRT` 两个零调用重载。
- [x] 2. `RenderTargetManager` 增加 RT 注册表 + `GetAliveCount()`，创建点登记入表。
- [x] 3. 统一 getter 文档化：标注唯一权威，其余标框架内部。
- [x] 4. 补 `doc/ecs/ecs_sub_world.md`（原为 0 字节；后随 ContextRole 门控体系删除而移除）。

阶段 A 落地明细：

- `RenderTargetManager` 新增 `registry` / `Destroy()` / `Find()` / `GetAliveCount()`，
  `Release()` 从空实现改为真正回收，析构函数兜底调用 `Release()`。
- 新增实例方法 `CreateOffscreenRT()`；两个 static `CreateRTFromGraphicsContext`
  退化为转发给它的便捷入口（阶段 B 再统一为 `Create(desc)`）。
- `CreateOffscreenRT` 复用成员 `CreateFBO`，删掉了原先内联的、与 `CreateFBO`
  逐行重复的 lambda 副本（`src/Vulkan/VKDeviceFramebuffer.cpp:27`）。
- 所有权约定生效：`OffscreenWorldRuntime` 析构由 `delete rt_` 改为
  `rt_manager->Destroy(rt_)`，避免 `Release()` 二次释放。
- 五个 RT getter 标注语义：`ECSContext::GetRenderTarget()` 为唯一权威，
  其余（`RenderContext` / `AppFramework` / `SwapchainModule` / `RenderTargetSystem`）
  标为框架内部接线或系统内缓存。

验证：以 build 目录中的真实编译命令行（MSVC 19.51，`/std:c++20`）对
`RenderTargetManager.cpp`、`RenderToTexture.cpp`、`AppFramework.cpp`、
`SwapchainModule.cpp`、`GraphicsContext.cpp`、`Context.cpp`、
`RenderTargetSystem.cpp`、`WorkManager.cpp`、`WorkObject.cpp` 做 `/Zs` 语法检查，全部通过。

### 阶段 B — 描述子与工厂

- [x] 5. 引入 `RenderTargetDesc` + `RenderTargetManager::Create(desc)` + `RenderTargetHandle`。
- [x] 6. `OffscreenWorldRuntime` 改用新 API 创建 RT。
- [x] 7. 类改名 `RenderTarget` → `OffscreenRenderTarget`，旧名以别名保留。

阶段 B 落地明细：

- 新增 `inc/hgl/graph/render/RenderTargetDesc.h`：声明式描述子，含
  `kind / name / width / height / color_formats / depth_format / has_depth / samples /
  resizable / fence_count / clear_color / env_profile`，并提供
  `OffscreenColorDepth()` / `OffscreenColorOnly()` 两个工厂预设与 `IsValid()`。
  设备相关的默认格式**不在 desc 中硬编码**，由 `Create()` 按 `VulkanDevAttr` 解析。
- `RenderTargetManager::Create(desc)` 返回 `RenderTargetHandle`
  （`std::unique_ptr<IRenderTarget, RenderTargetDeleter>`），释放时自动向 Manager 注销。
- 类改名：`RenderTarget` → `OffscreenRenderTarget`，新头文件
  `VKOffscreenRenderTarget.h`；旧 `VKRenderTargetSingle.h` 退化为转发头，
  `VK.h` 中保留 `using RenderTarget = OffscreenRenderTarget;` 兼容一个版本周期。
- `clear_color` / `env_profile` 收归 RT：`IRenderTarget` 新增 `SetClearColor` /
  `GetClearColor`（`env_profile` 原已有），由 `Create()` 从 desc 写入。
- `OffscreenWorldRuntime` 全面改用新 API：不再手写取 `surface_format` +
  `GetDepthFormat()`，改为 `RenderTargetDesc::OffscreenColorDepth(w, h, prefix)`；
  成员改用 `RenderTargetHandle`，析构只需 `rt_.reset()`；
  `RenderOnce()` 新增无参重载，直接取 RT 上的清屏色。
- `RenderToTexture.cpp` 示例改为把清屏色声明在 `OffscreenWorldConfig::clear_color`，
  调用 `runtime.RenderOnce()` 不再重复传色。

验证：9 个 TU 的 `/Zs` 语法检查全部通过（方法见 MEMORY.md）。

已知限制：`samples != 1` 时 `IsValid()` 直接返回 false（MSAA 未实现，留待阶段 D），
避免"声称支持但静默降级"。

### 阶段 C — OffscreenWorld 入引擎

- [x] 8. 新增 `OffscreenWorld`（引擎内），`OffscreenWorldRuntime` 下线。
- [x] 9. 新增 `ECSContext::RenderTo(rt, clear, dt)`，删除 `RenderOnce` 手写十步。
- [x] 10. 改写 `example/Basic/RenderToTexture.cpp`，作为新 API 的样板。
- [ ] 11. `clear_color` 在 `WorkObject` / `RenderSystemCore` 上的重复存储保留为便捷入口
        （Delegate 而非删除），避免破坏既有示例；权威值已在 RT 上。

阶段 C 落地明细：

- 新增 `ECSContext::RenderTo(rt, clear, dt)` / `RenderTo(rt, dt)`。
  内部复用 `BeginManagedRenderFrame` + `RenderDrawOnly` + `EndManagedRenderFrame`，
  **与主窗口路径共用同一套帧驱动**，不新增平行实现。
  为支持离屏，`BeginManagedRenderFrame` 增加 `need_swapchain_acquire` 参数
  （离屏 RT 无 swapchain 图像，跳过 `AcquireSwapchainImage`）。
  调用期间临时切换 `render_target` / `clear_color`，结束后恢复。
- 新增引擎内 `OffscreenWorld`（`inc/hgl/graph/module/OffscreenWorld.h`
  + `src/SceneGraph/module/OffscreenWorld.cpp`，已加入 CMake）。
  一行 desc + 一次 `Create()` 即可获得 RT + ECSContext + 渲染系统；
  `Render()` 内部走 `RenderTo()`，不再手写十步。
- 顺带修掉一个隐患：子世界的 `RenderTargetSystem` 会把共享 `RenderContext` 的
  current RT 改写为离屏 RT 且不恢复。`OffscreenWorld::Render()` 结束后显式恢复
  为主世界 RT（`RestoreMainRenderContext()`）。
- `example/common/OffscreenWorldRuntime.h` 已删除（连同空的 `example/common/` 目录），
  其能力全部由引擎 `OffscreenWorld` 承担。
- `example/Basic/RenderToTexture.cpp` 改用引擎 `OffscreenWorld`。

验证：10 个 TU 的 `/Zs` 语法检查全部通过（含新建的 `OffscreenWorld.cpp`，
该文件复用同 target 的编译选项构造命令行）。

### 阶段 D — resize 与 RenderGraph 集成

- [x] 12. `RenderTargetManager` 按 desc 真正重建 texture + FBO（Swapchain 仍由 `SwapchainModule` 负责）。
- [x] 14. 清理 `src/GUI/ThemeEngine.cpp` 的死路径（并修掉其中一处空指针解引用）。
- [ ] 13. RenderGraph 跨 RT pass 链 —— **拆分为独立后续任务**，原因见下。

#### 12 已完成：真 resize

- `registry` 条目改为持有 `RenderTargetDesc`，`Resize()` 按 desc 重建。
- 抽出了 `CreateAttachments()`，创建与重建共用同一段逻辑。
- **重建只换纹理与 FBO**，`queue` / `cmd_buf` / `render_complete_semaphore` **复用**。
  原因：`RenderTargetData::Clear()` 明确不释放这三者（由设备侧持有），
  若每次重建都重新 `CreateQueue` / `CreateRenderCommandBuffer`，设备对象会累积泄漏。
- `desc.follow_window`（默认 false）：只有显式声明的 RT 才在 `GraphicsContext::OnResize`
  时跟随窗口重建。离屏 RT 尺寸通常与窗口无关（如固定 512x512 的 RTT），不该被连带改变。
- `OffscreenWorld::Resize(w, h)` 重建后重新接线 `RenderTargetSystem` / `CameraSystem` /
  `ECSContext` 的 RT 引用。
- **约束（必须遵守）**：重建后 `Texture2D` 指针会变化，持有旧指针者必须重新
  `GetColorTexture()` 并重新绑定材质，再重新 `Render()` 才有内容。

#### 13 未实现：跨 RT pass 链需要先行改造

`RenderGraph::Pass::renderTarget` 字段一直存在，但 `ExecuteRenderGraphPasses()`
**从未读取它** —— 是个死字段。真正生效需要：

- 命令缓冲由 `RenderSystemCore::BeginFrame()` 从**帧初始 RT** 获取，每个 RT 持有自己的
  `cmd_buf`；跨 RT 切换时必须在切换点做一次
  `EndRendering/EndRender → BeginRender/BeginRendering`，并处理跨 RT 的提交与信号量同步。
- 这属于"多段渲染"架构改动，且**当前没有多 RT 用例可供验证**（RenderToTexture 是单 RT），
  草率实现会留下"看似能用实则画错目标"的隐患。

因此本阶段只做了**消除静默失败**这一步：字段加了明确文档说明，
执行器在检测到"pass 请求了非当前 RT"时输出 `LogWarning` 而不是静默忽略。
真正的多 RT pass 链（MRT / 后处理链 / 阴影图）留作独立任务，需：
1. `RenderSystemCore` 支持多段 `Begin/End` 与跨 RT 提交同步；
2. ~~引入 `RenderPassRequest` 明确描述 `world / target / clear / camera_override`；~~ **已完成**：
   `inc/hgl/ecs/core/RenderPassRequest.h:31` + `ECSContext::RenderTo(const RenderPassRequest&)`
   （`src/ecs/core/Context.cpp:445`），见 §3.4 校对注；
3. 补一个多 RT 用例（如后处理链）作为验证基线。

#### 14 已完成：GUI 死路径

`src/GUI/ThemeEngine.cpp` 不止 `device->CreateRT` 一处问题，实际有 7 处以上编译错误
（`!old_rt` 后解引用 `old_rt`、新 RT 覆盖旧 RT 未释放、`Render()` 缺 return、
`CreateForm` 少传参数、`GetDefaultThemeEngine()` 少传 dev 等），整体已腐朽且
`ThemeEngine` 只持有 `VulkanDevice`、拿不到 `GraphicsContext`，无法改用新 API。

处理：未删除（不可逆，GUI 可能后续重启），而是在文件头加了完整问题清单与重写指引，
并修掉那处空指针解引用。启用 GUI 前须以 `RenderTargetDesc` + `OffscreenWorld` 为基线重写。

---

## 五、阶段 E — depth-only 目标（shadow map）

阶段 B 的描述子只能表达"有颜色"，`Create()` 在 `color_formats.empty()` 时无条件补一个
设备默认颜色格式，因此**零颜色附件无法表达**。而 shadow map 正是典型形态。

底层其实早就支持，缺的是上层描述能力——确认过这几处都能正确吃 0 颜色附件：

- `CreateFBO()` 的 `att_count = color_count + (depth?1:0)`
- `BeginRendering()` 的深度 barrier 放在 `barriers[color_count]`
- `FramebufferInfo::GetAttachmentCount()`、`CreateAttachmentDescription()` 的附件数计算
- `SubpassDescription(color_refs, 0, &depth_ref)`

### 改动清单

**1. 描述子（`RenderTargetDesc.h`）**

- 新增 `has_color`（与 `has_depth` 对称），`OffscreenDepthOnly()` 工厂，
  以及可选的 `depth_format` 参数（三个工厂都支持）
- `IsValid()` 新增两条校验：不能既无颜色也无深度；声明无颜色时不得给颜色格式

**2. 创建路径（`RenderTargetManager`）**

- 抽出 `ResolveFramebufferInfo()`，`Create()` 与 `RebuildOffscreenRT()` 共用——
  原先两处各写一份解析逻辑，改一处必须记得改另一处
- `has_color` 为假时保持零颜色附件，不补默认格式
- `RebuildOffscreenRT()` 调整为**先解析后释放**：原顺序是先释放旧纹理再取设备属性，
  设备不可用时会留下"资源已释放但没重建"的残废 RT
- `CreateAttachments()` 顺手修掉一个隐患：原先用 `AutoDeleteObjectArray<Texture2D>`
  兜底，失败路径会 `delete` 纹理，而纹理归 `TextureManager`——改为
  `AutoDeleteArray`（只释放数组）+ 失败路径显式 `Release()`

**3. 管线解析（`VKPipelineResolver.cpp`，三处）**

| 位置 | 原行为 | 现行为 |
|---|---|---|
| `ValidateResolveRequest` | `color_attachment_count == 0` 直接拒绝 | 零颜色时要求必须有深度 |
| `HasCompleteFinalKey` | 同上，key 视为不完整 | 同上判据 |
| `MaterializePipeline` | `stencilAttachmentFormat = depth_format` | 仅当格式确实带模板位时才声明 |

第三处是隐藏的 validation 错误：对 `D16_UNORM` / `D32_SFLOAT` 这类纯深度格式
（shadow map 常用），`stencilAttachmentFormat` 必须为 `VK_FORMAT_UNDEFINED`。

**4. 布局转换（`VKCommandBufferRender.cpp`）**

`EndRenderingPresent()` 原先只在 `color_count > 0` 时发 barrier，depth-only 目标的深度
会停在 `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`——**采样即为非法**。现在：

- 按 `IRenderTarget::IsSwapchain()`（新增虚函数）分流
- 交换链：颜色 → `PRESENT_SRC_KHR`（维持原语义）
- 离屏：颜色与深度都 → `SHADER_READ_ONLY_OPTIMAL`。
  深度也走 SRO 而非 `DEPTH_STENCIL_READ_ONLY_OPTIMAL`，因为采样侧
  `BindlessTextureManager` 的 descriptor `imageLayout` 固定为 SRO，两者必须一致
- `BeginRendering()` / `EndRenderingPresent()` 的深度 `aspectMask` 改为按格式判断：
  纯深度格式不能带 `STENCIL` 位

**5. 深度清屏值（`RenderSystemCore.cpp` + `VKCommandBufferRender.cpp`）**

两处相关：

- `BeginRendering()` 显式把深度槽置为 `1.0f`。`VkClearValue` 零初始化的 `depth` 是
  `0.0f`，而 `hgl_align_realloc` 用的是 `_aligned_realloc`（**不初始化新空间**），
  原值实际是未初始化内存；在默认 LESS 深度测试下清成 0 会让 depth-only 目标全空
- `RenderSystemCore::BeginRenderPass()` 在 `GetColorCount() > 0` 时才写颜色 clear 值。
  `VkClearValue` 的 `color` 与 `depthStencil` 是同一 union，depth-only 时
  `clear_values[0]` 就是深度槽，写颜色会覆盖深度清屏值

**6. `OffscreenWorld`**

`OffscreenWorldDesc` 新增 `depth_only` 与 `depth_format`，按标志选择
`OffscreenDepthOnly()` 或 `OffscreenColorDepth()`。

### 验证用例

新增 `example/Basic/ShadowMap.cpp`：离屏世界 `depth_only = true` + `PF_D32F`，
以光源视角渲染球体到 depth-only RT，主世界的立方体直接以该深度纹理作 albedo 采样显示。
日志逐项打印 `color_count` / `has_depth` / 深度纹理指针 / 格式 / 布局 / 尺寸，
并显式判定布局是否为 `SHADER_READ_ONLY_OPTIMAL`（可采样）。

实跑结果（Debug + Khronos validation layer）：

| 检查项 | 实测 |
|---|---|
| 零颜色附件目标 | `color_count=0`、`has_depth=1` |
| 深度格式 | `126`（`VK_FORMAT_D32_SFLOAT`，纯深度） |
| 深度布局（渲染前） | `3` = `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |
| 深度布局（渲染后） | `5` = `SHADER_READ_ONLY_OPTIMAL`（可采样） |
| 采样注册 | `BindlessTextureManager` 以 `handle=1` 注册该深度纹理 |
| validation | 无 ERROR |

唯一一条 validation 提示是 `MsgCode:0`（非错误）：片段着色器写出的 `outColor`
因目标没有颜色附件而被丢弃。这是 depth-only 渲染的**预期行为**——当前材质管线
是共享的；若后续要消除该提示，需要引入不写颜色的深度专用管线。

### 构建注意（踩过的坑）

`IRenderTarget` 是全渲染链的公共基类。给这类接口新增虚函数后，
MSBuild 的增量构建**可能漏编部分源文件**（实测漏编 `VKSwapchainRenderTarget.cpp`），
造成 vtable 与新接口错位，表现为**框架初始化早期段错误**（`exit=139`、
日志停在主窗口 `RenderPass` 创建之后、无任何业务日志）。

两条约束：

1. 新增虚函数一律声明在接口**末尾**——插在中间会整体平移后续槽位，
   任何未重编的旧目标文件都会错位。
2. 改这类头文件后，`grep -rl` 找出所有直接包含它的源文件（当前 25 个）并
   `touch` 强制重编，不能只依赖增量依赖检查。

### 未做（后续）

- **多层 / cubemap**：点光源阴影（cubemap 6 面）与 CSM（array）需要
  `layers` 字段，且 `CreateFBO` 的 `fb_info.layers = 1`、`BeginRendering` 的
  `ri.layerCount = 1` 都是硬编码，需一并改为变量。本次按单层 2D 实现。
- **深度比较采样**：shadow map 的 `sampler2DShadow` 需要比较采样器与
  `DEPTH_STENCIL_READ_ONLY_OPTIMAL` 路径，本次只保证普通采样可用。

---

## 六、验收标准

- 新建一个离屏 RT 并渲染一帧，应用侧代码 ≤ 5 行（当前约 270 行含 `OffscreenPass`）。
- 全仓只有 1 个 RT 创建入口、1 个权威 getter。
- `OffscreenWorld` 支持 resize，且 resize 后主场景纹理引用自动更新。
- 对象追踪器在 `RunFramework` 退出时报告 0 RT 泄漏。
- `example/Basic/RenderToTexture.cpp` 可作为文档级样板直接引用。

---

## 七、风险与注意

- **改动面**：`IRenderTarget` 是全渲染链的公共基类，改名需保留 typedef 过渡，避免一次改动过大。
- **离屏与主路径共用同一 RenderCore**：`RenderSystemCore` 目前在 `:31/:45/:106` 三处重新取
  `render_target` 以规避 resize 重建，标准化后应收敛为一次注入。
- **`Example` 与引擎的边界**：`OffscreenWorld` 入引擎后，示例不再承担"框架能力"职责，
  这是本次标准化最大的收益点。
