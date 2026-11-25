# 层间契约（下层需要的接口由上层实现）

本文登记 `2026-11` 重组（`feature/ecs-scenegraph-restructure`）引入的**下层契约**，
以及 Vulkan 层剩余反向 include 的复核结论。目的是让"下层不 include 上层管理模块"
这条纪律有单一台账，改接口时不用重新考古。

## 1. 契约清单

| 契约头 | 声明方（下层） | 实现方（上层） | 下层为何需要它 |
|---|---|---|---|
| `hgl/vk/VKTextureOwner.h` | `src/Vulkan/VKTexture.cpp` | `graph::TextureManager` | 纹理析构要通知 bindless 经理并从所有者表注销；伴生 view 要 `VkDevice` |
| `hgl/vk/VertexBufferAllocator.h` | `src/Vulkan/VertexDataManager.cpp` | `graph::BufferManager` | 顶点/索引缓冲池要创建 VAB/IBO 并在析构时归还 |
| `hgl/vk/UploadLaneProvider.h` | `src/Vulkan/VKSwapchainRenderTarget.cpp` | `graph::TextureManager` | 交换链提交要把上传车道串进等待列表 |

共同设计约束：

- **只暴露值/句柄，不暴露上层类型**。三个契约的返回类型都是 Vulkan 侧已存在的
  句柄或纯值结构，因此实现方（上层）能自由变化，下层不受影响。
- **方法名刻意与继承来的同名成员区分**（如 `GetOwnerVkDevice` / `CreateAllocatorVAB`）。
  实现方多半同时继承 `GraphModule`，同名会产生重载歧义 —— 这是实测踩到的编译错误。
- **`std::source_location` 透传**：`VertexBufferAllocator` 保留调用点位置参数，
  否则泄漏报告会把所有分配都归到契约层而不是真正的调用点。

## 2. Vulkan 层剩余反向 include 的复核结论

以下 include 仍在 `src/Vulkan` / `inc/hgl/vk` 内出现，但**都不是上层管理器依赖**，
而是共享**数据/契约类型**：

- `hgl/graph/data/BitmapData.h`、`ImageRegion.h`、`hgl/graph/ubo/ViewportInfo.h`、
  `hgl/graph/ubo/EnvironmentInfo.h`、`hgl/graph/CameraInfo.h`、`hgl/graph/mesh/Primitive.h`、
  `hgl/graph/font/Font.h`、`hgl/graph/geo/GeometryVertexFormat.h`、
  `hgl/graph/ShaderBufferSource(s).h`、`hgl/graph/texture/TextureLoader.h`

判定依据：这些头里没有管理器类、没有生命周期协调逻辑，只有 POD/值结构与纯函数声明
（`DebugCaptureQueue` 与 `FrameDebugger` 仅读 `CameraInfo` 的字段做诊断）。

**已消除的管理器依赖**（重组前存在，现已不存在）：

- `VKTexture.cpp` → `TextureManager.h` / `GraphicsContext.h`（改为 `TextureOwner`）
- `VertexDataManager.cpp` → `BufferManager.h`（改为 `VertexBufferAllocator`）
- `VKSwapchainRenderTarget.cpp` → `GraphicsContext.h` / `TextureManager.h` /
  `GpuUploadQueue.h`（改为 `UploadLaneProvider`）

把这些数据头搬去中立层属于**跨层类型迁移**，影响面远大于本次范围（`BitmapData`
等被 200+ 处引用），因此不在本次重组内做；若后续要做，应一次性迁移并同步公开头。

## 3. 上层对下层的依赖方向（正常方向，无需改动）

`ECS → SceneGraph → Vulkan`，以及 `WorldAssembly → {ECS, SceneGraph, Vulkan}`。
聚合入口 `GraphicsContext` / `ECSContext` 位于上层，拥有下层对象；
下层只通过上面的契约回调上层，**不持有**上层指针（`TextureOwner` 等只在
析构/提交这类瞬时路径上被调用）。
