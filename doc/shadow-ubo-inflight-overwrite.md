# ShadowInfo 跨帧覆写：阴影逐帧左右/远近跳

> 2026-09 结案。示例 `example/Basic/CascadeShadowMap.cpp` 拖拽视角时，阴影会一帧偏左、一帧偏右，贴合距离也一帧近、一帧远。静止后稳定。RenderDoc 连续截帧永远正常；把帧率降到约 1fps 时非常明显。

## 1. 症状签名

| 观察 | 含义 |
|------|------|
| 只在相机移动/旋转时出现 | CPU 每帧写出的级联矩阵在变。静止时前后帧矩阵相同，覆写没有可见副作用 |
| 一帧正常、一帧错位，像奇偶帧切换 | 不是矩阵算错，而是 GPU 读到了「上一帧或上上一帧」的 `ShadowInfo` |
| RenderDoc 截帧永远正常 | 截帧把 CPU/GPU 串行化，写的时候没有在途帧 |
| 1fps 更明显 | 帧间隔放大了交替，不是「偶发竞态」 |
| 按住 F1 排空全部在途 fence 后立刻消失，松开复现 | 定性实验：问题在「写入时机」，不在拟合公式 |

CPU 侧已经排除。拖拽探针下静态级联在缓存命中期间 `shadow_vp` 最大元素差为 0，`cache_origin` 漂移不超过 1e-4 m。控制器的重绘分支和命中分支写的是同一组 UBO 字段。

## 2. 根因

交换链 `slot_count == image_count`。本引擎创建交换链时把图像数抬到至少 3（`SwapchainModule.cpp`），运行时日志确认 `frames_in_flight=3`。`SwapchainRenderTarget::NextFrame()` 只等**当前槽**的 fence，再 acquire 一张已经空闲的图像。因此最多有 2 帧主通道还在 GPU 上跑。

`ShadowInfo` 原来是**一份** host-visible 映射（`StructView::MapInternal()` = `MapWindow(0, 整块)`）。两条路径都会在主帧还在读的时候整块覆写：

1. `EnvironmentSystem::MarkShadowDirty()` → `EnvironmentManager::MarkDirty()`。示例在 `Tick()` 里、交换链 acquire **之前**调用。此时上一帧主通道可能还在采样。
2. `ViewUBOCommitSystem` → `CommitMaterialized()`。主帧 acquire 之后会再写一次，但另外两个槽仍在飞，写的还是同一块内存。

主帧片元用 Scene Set binding 5 的 `shadow_vp` 把世界坐标变到光空间，再采样已经画好的阴影贴图。贴图是用「这一帧的矩阵」画的（离屏 `RenderTo` 自己有 fence），UBO 里却可能已经是下一帧的矩阵。结果就是阴影相对遮挡体跳一格：左右是光空间 UV 变了，远近是深度比较基准变了。

`Context::RenderTo` 前后两次 `WaitFence()` 是同一类踩踏的先例，注释写的是「shadow map 被画成主相机视角（间歇性阴影闪烁丢失）」。那次保护的是 Camera UBO。Shadow UBO 没有同等保护。

`BufferUpdateClass::Deferred` 的注释写着「可延迟到下一帧」，但全库没有任何消费者实现这个语义。`CreateUBO` 的 `Auto` 策略要么 ReBAR 直写，要么单块 staging，都不是 per-frame ring。

Push Descriptor 本身没问题：`vkCmdPushDescriptorSet` 记进命令缓冲，后一帧改绑定不会改已经录好的命令。错的是所有帧推的是**同一块** `VkBuffer`。

## 3. 修复

`ShadowInfo` 按 acquired image 分槽，不靠每帧排空 fence（那会把 3 帧流水线打成 1 帧）。

- 每份 profile 物化 `kShadowUboRing = 8` 个独立 UBO（`ShadowUBO:<name>:<slot>`）。8 盖住常见 `image_count`；运行时实际在飞的是 3。
- `MarkDirty()` **不再写** shadow GPU。CPU 权威仍是 `Profile::cpu.shadow`。Tick 里的 `MarkShadowDirty()` 只保证 CPU 数据是新的。
- 只有当前 RT 是交换链时，`ViewUBOCommitSystem` 才调用 `CommitMaterialized(acquired_image, true)`。`NextFrame()` 已经等过这张图像的上一帧，这个槽可以写。其它在途帧读自己的槽。
- 离屏 shadow pass 不写任何槽。它发生在主帧 acquire 之前，`GetCurrentFrameIndex()` 默认是 0，此时写 slot 0 会踩仍在飞的主帧。深度 pass 不采样 `ShadowInfo`。
- `RenderSceneUBOSystem::ResolveShadowUBO()` 在 `RenderFrameSync`（commit 与 upload 之后）按同一 `acquired_image` 绑定。Push Descriptor 把该槽的 `VkBuffer` 记进本帧命令缓冲。

sky 仍是单份，不在本次范围。Camera UBO 同样是单份；`RenderTo` 用全槽 `WaitFence()` 挡住了离屏 pass 那条路径。主帧相机数据若再出现「整幅画面逐帧跳」而阴影不跳，按同一办法分槽，不要先怀疑 CSM 拟合。

## 4. 以后不要做的事

- 不要在 `MarkDirty` / `MarkShadowDirty` 里把 `cpu.shadow` 写回 GPU。调用点在 acquire 之前。
- 不要在离屏 `RenderTo` 期间写 shadow ring 的任意槽。
- 不要用 `WaitFence()` 全槽排空来「修」这个跳变。能消症状，但每帧都把在途帧等完。
- 绑定用的 `frame_index` 必须等于本帧写入的槽。两边都取 `IRenderTarget::GetCurrentFrameIndex()`（交换链上即 `acquired_image`），不要一边用 `current_slot`、一边用 image index。二者不相等。
- `kShadowUboRing` 必须 ≥ 交换链 `image_count`。若以后把最小图像数抬过 8，先加大 ring，不能对 index 取模（取模会让两张在飞图像共用一槽，bug 原样回来）。

## 5. 相关文件

| 文件 | 角色 |
|------|------|
| `inc/hgl/graph/module/EnvironmentManager.h` | `kShadowUboRing`、`GetShadowUBO(id, frame_index)`、`CommitMaterialized(frame, commit_shadow)` |
| `src/SceneGraph/module/EnvironmentManager.cpp` | 分槽物化；`MarkDirty` 只写 sky |
| `src/ecs/systems/render/ViewUBOCommitSystem.cpp` | 仅交换链帧、acquire 之后写当前槽 |
| `src/ecs/systems/render/RenderSceneUBOSystem.cpp` | 按同一 image index 绑定 binding 5 |
| `src/Vulkan/VKSwapchainRenderTarget.cpp` | 只等当前槽；`GetCurrentFrameIndex()` = `acquired_image` |
| `src/SceneGraph/module/SwapchainModule.cpp` | `image_count` 至少 3，`slot_count == image_count` |
