# CSM 线遗留两项：线索与下一步（第 1 项已完成，第 4 项待办）

## 第 4 项：`[LEAK]` 报告为什么「间歇不可观察」

**根因（已定位）**：`CSM_AUTOWALK` 是**行走速度(m/s)**，不是帧数——见
`example/Basic/CascadeShadowMap.cpp:1077/1081`。示例没有帧上限、也没有自动退出开关，
headless 跑只会一直循环 ⇒ 常以 `timeout` 杀掉（rc=124），退出期泄漏报告**根本没机会打印**。
「间歇性 133 条 + exit 3」是因为只有手动关窗口那条路径才会走到退出期报告。

**下一步**：
1. 加自动退出开关（推荐框架级，示例也能受益）：例如 `HGL_EXIT_AFTER_FRAMES=N` 或
   `CSM_FRAMES=N`，在 `Tick` 计数到 N 后正常退出（走完整析构路径）。
2. 退出后抓报告：引擎已有**按类型汇总**，见 `src/Vulkan/VKDevice.cpp:205-227`
   （`VkBuffer/VkImage/VkDeviceMemory/VkImageView/VkSampler/VkFramebuffer/VkRenderPass/VkPipeline/
   VkPipelineLayout/VkDescriptorSet/VkDescriptorSetLayout` + UNKNOWN），明细行格式
   `[LEAK] Type: 0x%x Handle: 0x%llx Name: %s`（:232）。
3. 按汇总分类判定：
   - 若集中在 exchange/占位类型且对象随后析构 ⇒ **shutdown 顺序问题**（枚举早于持有者析构）：
     把枚举挪到所有持有者析构之后，或明确一次性 shutdown 顺序。
   - 若集中在某持有者（如 UBO/描述符池）⇒ **真泄漏**，修持有者。
4. 结论落地前，**退出码不可作为判据**（现在拿 rc 判绿会假红/假绿）。

**顺带实测（2026-09-27，做第 1 项冒烟时观察到，直接改写了本条的前提）**：
`CSM_CACHE_DIFF=1 CSM_AUTOWALK=4` 这条线**自己会收尾**——第 30 轮汇总 + 自查汇总后立刻
`AppFramework Destructor Start`，一次完整析构（实测 rc=**0**，不是 124），退出期报告照常打印。
所以「必须加帧上限开关才能拿到报告」的前提不成立：**先跑 CACHE_DIFF 线拿报告**即可
（AUTOWALK 单开那条才是死循环）。一次运行里报告**打了两次**：① `BufferManager::Release` 前
「163 objects still alive」；② `VulkanDevice` 析构末「108 objects still alive」（中间还有一句
`Found 4 undestroyed buffers, cleaning up...`）。两份明细合计 **271 行**，按类型：
`:VKBuffer` 202 / `:VKMemory` 15 / `:VKImage` 7 / `:VKImageView` 7 / `:VKFence` 5 / `:VKPipeline` 4 /
`:VKRenderPass` 2 / `:VKTextureCmdBuf` 3；按名字头部：`TextureReadback:Staging` 60、
`TransferSrcBuffer` 52、`Texture_*` 21、`SSBO:ECS:Batch:*` 36、`VAB_*` 9、`StorageBuffer` 8。

**根因线索（未结案，证据已足）**：`VkBuffer` 这一类**基本全是假阳性**——注册侧到处都在
`TrackBuffer`/`TrackObject(VK_OBJECT_TYPE_BUFFER,…)`，但**注销侧只有一处**
（`src/Vulkan/buffer/StagedBuffer.cpp:37`，注销它自己的 staging buffer），
`BufferOwner::~BufferOwner()`（`src/Vulkan/buffer/BufferOwner.cpp:7-21`）销毁了 VkBuffer/DeviceMemory
却**从不** `device->UntrackObject(VK_OBJECT_TYPE_BUFFER, …)` ⇒ 每个 DeviceBuffer 型缓冲都永久留在
`VulkanDevice::tracked_objects` 里，退出期照单报「泄漏」。对照证据：报告里 60 条
`TextureReadback:Staging` 与该次运行的**读回次数正好 1:1**（30 轮 × 每轮 2 次读回，
`CascadeShadowMap.cpp:368/425` 两处 `ReadbackCascadeDepth`），而 `VKTextureReadback.cpp` 的
`delete staging` 是无条件执行的；且 Image/ImageView/Sampler/Fence/DeviceMemory 这些**有**
`UntrackObject` 的类型在明细里只有个位数 ⇒ 数量级差异正是缺注销所致。
**下一步**：给 `~BufferOwner`（或各工厂的释放路径）补 `UntrackObject`，再跑一轮看 202 条
VkBuffer 归零；归零即本条结案（报告可信），未归零的那部分才是真泄漏。

## 第 1 项：交换链颜色图帧外读回（真 VUID）—— ✅ 已完成（2026-09-27）

**现状（改造前）**：`example/Basic/AlphaTestShadow.cpp:309-311` 的 `graph::ReadbackColorTarget(main_rt, ...)`
读的是交换链颜色图（真实布局 `PRESENT_SRC_KHR`，见 `:296-300` 注释）；引擎侧
`src/Vulkan/VKTextureReadback.cpp:51-53` 把 `PRESENT_SRC_KHR` 当「渲染刚写过」处理，
于是在 **acquire 窗口之外**对该图做布局转换并提交 ⇒
`vkQueueSubmit(): presentable VkImage ... has not been acquired`（PASS 时同样存在）。

**关键约束（已核实）**：acquire/present 由交换链 RT 在**渲染阶段内**开合——
`src/Vulkan/VKSwapchainRenderTarget.cpp:74-104` 是 `vkAcquireNextImageKHR`，
`:181-187` 是 `Present`。而示例的读回发生在**帧之后**（`AlphaTestShadow.cpp:739` 取
`ecs_context->GetRenderTarget()` 后读回）。⇒ 只把读回「挪进 Tick」不够：
`AlphaTestShadowApp` 虽是 `WorkObject`（有 `Tick(double)` 每帧回调，`:657`），
但 acquire 窗口的开启在渲染提交阶段，示例侧无法从外部进入该窗口。

**落地（两步一起做）**：
1. **引擎侧护栏**：`IRenderTarget` 新增 `IsColorReadbackWindowOpen()`
   （`inc/hgl/vk/VKRenderTarget.h`，末尾追加虚函数；离屏 RT 恒 true）。交换链 RT 覆写返回
   `acquire_window_open`——`NextFrame()` 取得 image 后置真、`Submit()` 收尾（present 之后、
   以及提交失败/acquire 失败路径）置假（`inc/hgl/vk/VKRenderTargetSwapchain.h` /
   `src/Vulkan/VKSwapchainRenderTarget.cpp`）。`ReadbackColorTarget` 对窗口外的请求
   **直接报错拒绝**（`src/Vulkan/VKTextureReadback.cpp`），把「帧外读回交换链颜色图」从可用能力里去掉。
2. **帧内回读窗口**：`SwapchainRenderTarget::SetInFrameReadbackHook(fn)`——在
   `Submit()` 里「本帧队列提交之后、`vkQueuePresentKHR` 之前」执行一次（一次性，执行后清除；
   提交失败的帧把钩子留到下一次成功窗口）。示例 `RequestColorDump()` 用该钩子读回，
   钩子内照常调 `graph::ReadbackColorTarget`。注意**帧号口径前移一帧**（旧实现 Tick 帧 N 读到的是
   渲染帧 N-1，现在请求帧 = 读到的渲染帧）⇒ D3 相位请求点 1/4/8 → **1/3/7**，帧 8 结算。

**验证（2026-09-27 实跑）**：
- `ATS_SELFCHECK=1` 连跑 **5 次**：rc=0；校验层消息 **0**（改造前每次运行 3 条
  `not been acquired` + 6 行回显）；D1 `bbox=112x58 filled=3740 57.6%`、D3 `18189 px / 0 px` +
  `600662 px`、D4 拒绝 0、selfcheck PASS——与改造前**逐位一致**；`mean_lum=113.3/112.3/134.0` 不变。
- 对照组 `ATS_D3_NOKNOB=1`：0 px 噪声底、rc=0、0 校验层消息。
- **破坏验证**（临时把示例改回帧外读回）：引擎护栏报错 3 次 + 新增的
  「帧内读回不完整（A/B/C 有效大小 0/0/0）」判负 + **rc=1**，且 0 校验层消息
  （非法提交根本没发生）⇒ 契约与护栏都有牙；`0 越界`。
- `TestCSMIncrementalPass` **21 Passed**/rc=0（Test 21 追加源码契约：窗口状态 + 钩子 +
  读回侧窗口外拒绝 + 示例改用该窗口，缺任一条即失败）。
