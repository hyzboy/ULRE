# CSM 线遗留两项：线索与下一步（第 1/4 项）

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

## 第 1 项：交换链颜色图帧外读回（真 VUID）

**现状**：`example/Basic/AlphaTestShadow.cpp:309-311` 的 `graph::ReadbackColorTarget(main_rt, ...)`
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

**方案（两步一起做）**：
1. **引擎侧护栏**：`ReadbackColorTarget` 对「处于非 acquire 态的交换链图」直接报错/fail-fast。
   需要：能识别交换链图（RT/纹理上加标记或查交换链拥有关系），并查询当前 acquire 窗口状态
   （交换链侧已有 `acquired_image` 状态）。
2. **提供帧内回读窗口**：在「渲染提交之后、Present 之前」给应用一个回读钩子
   （或让回读请求排入本帧命令缓冲），示例改用该钩子读回。

**验证**：ATS 三契约数字与 selfcheck 不变、**0 VUID**（这条 VUID 必须消失）、0 越界。
