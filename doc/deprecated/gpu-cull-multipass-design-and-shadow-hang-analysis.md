# GPU 剔除多 Pass 架构设计与无阴影场景 GPU 挂起技术分析

> **文档标识**：`doc/gpu-cull-multipass-design-and-shadow-hang-analysis.md`  
> **编写日期**：2026-10-10  
> **涉及模块**：`GPUCullSystem`、`PrimitiveBatchPipeline`、`entity_gpu_cull.glsl`、`DeviceQueue`  
> **关联合约**：`doc/future/entity-rows-gpu-cull-plan.md`（§11 T4c 系列计划）

---

## 1. 故障现象与定位过程

### 1.1 初始症状
在运行轻量级示例（如 `example/GettingStarted/DrawTriangle.cpp`）时，程序运行到第 3 帧（准备复用帧槽 0 时）发生严重超时与崩溃：
- `[FENCE] WaitLastSubmit FAILED res=2 (VK_TIMEOUT)`：交换链槽 0 的 fence 在 1 秒超时内未完成。
- 随后触发 `vkQueueSubmit2 failed result=-4 (VK_ERROR_DEVICE_LOST)`，GPU 进入设备丢失状态。
- 后续帧因设备丢失引发连锁反应（Fence 在使用中被 Reset、交换链图像 Acquire 越界等）。

### 1.2 逐步收敛证据链
1. **系统隔离**：
   在 `DrawTriangleApp::InitECS()` 中临时调用 `gpu_cull->SetEnabled(false)`，跳过 `GPUCullSystem::Update()` 后，程序**不再卡死、Fence 正常同步**。但由于渲染已强制 GPU-Driven 接管（CPU ICB 生成已彻底废除），画面为空白。
2. **派发隔离**：
   在保留 `GPUCullSystem::Update` 的两处 `FillBuffer` 与队列提交的情况下，仅跳过 `cmd->Dispatch(group_count, 1, 1)`，Fence 同样不再超时。确认问题出在 **Compute Shader 的 GPU 执行期**。
3. **着色器逻辑缩小**：
   在 `entity_gpu_cull.glsl` 中进行断点式测试：
   - 入口直接 `return`：不卡顿；
   - 逐步执行至主帧（`blk == 0`）结束：**不卡顿，且三角形顺利渲染上屏**；
   - 一旦进入 `for (uint blk = 0u; blk < ULRE_ARENA_PASSES; ++blk)` 的第 2 轮（`blk == 1`，阴影块），GPU 必定超时挂起。

---

## 2. 架构背景：为什么主帧渲染与阴影 Pass 合并派发？

在传统的渲染管线中，主渲染与阴影通常分别派发不同的剔除 Pass，而 ULRE 在 **§11 T4c** 阶段将其重构成“单次 Compute 派发同时产出两 Pass 间接命令”，原因如下：

### 2.1 彻底淘汰 CPU 绘制路径，实现 100% GPU-Driven
- **历史瓶颈**：此前主帧已进入 GPU-Driven，但阴影 Pass 仍依赖 CPU 端遍历实体、排序并逐帧写入 ICB 与参数表。这迫使引擎在 CPU 侧保留沉重的批次对象管理与 ICB 生成逻辑（`WriteMeshDrawCommands` 等）。
- **T4c 目标**：把阴影 Pass 的间接命令也移交 GPU 异步生成，使 CPU 端所有传统 ICB 生成代码彻底清理（结案于 T4c4）。

### 2.2 避免对全局实体表的重复遍历与管线切换
- 场景中所有实体统一在世界的 `EntityGPUTable`（4096 项）中管理。
- 若主帧与阴影各派发一次 Compute：
  1. 需要两次管线绑定、两套 PushConstant 下发及额外的队列依赖切换；
  2. 4096 个实体的状态、Transform 和 4-ID 数据需要在显存中被重复读取两次。
- **单派发收益**：每个计算线程只读取一次实体行，在同一着色器中同时解算主帧与阴影的可见性与命令装配。

### 2.3 共享竞技场（Arena）分区隔离
虽然两 Pass 在同一个 Compute Shader 中运行，但数据在逻辑与物理上完全解耦：
- **间接命令与输出三表（8192 行）**：
  - `Pass 0（Main）`：走视锥剔除，写前半扇区（`row 0 ~ 4095`）；
  - `Pass 1（Shadow）`：走直通档（只需深度，绘制序无关），写后半扇区（`row 4096 ~ 8191`）。
- **元数据分块**：
  - 批标签表 `batch_tag`、批布局表 `batch_info`、计数表 `batch_counter` 均按 Pass 独立分块（`kArenaPasses = 2`），互不污染。
- **绘制消费**：
  - 绘制侧在主渲染和阴影阶段分别通过 `MaterialBatch::gpu_slice[pass]` 获取各自扇区的偏移（`row_base` 与 `count_buffer_offset`），两 Pass 渲染各取所需。

---

## 3. 故障根因分析

在架构上两 Pass 的数据存储已经分块，但**控制流判定维度存在疏漏**：

1. **PushConstant 未传递阴影批数**：
   C++ 侧维护了主帧批数 `batch_count_[0]` 与阴影批数 `batch_count_[1]`。但 `CullPushConstants` 结构中仅包含一个 `batch_count` 字段，且只填入了主帧批数（`batch_count_[0]`）：
   ```cpp
   pc.batch_count = batch_count_[0];
   // 未传递 batch_count_[1]
   ```
2. **GLSL 误判阴影批次有效**：
   在完全不需要阴影的示例（如 `DrawTriangle`）中，`batch_count_[1] == 0`。
   但在着色器中：
   ```glsl
   const uint batch1 = BatchTagRef(pc.batch_tag_addr).tags[1u * pc.max_visible_count + entity_index];
   const bool tagged1 = (batch1 != 0xFFFFFFFFu && batch1 < pc.batch_count); // 误用主帧批数校验！
   ```
   由于 GPU 显存端 Pass 1 的对应槽位读出 `batch1 == 0`，而主帧批数 `pc.batch_count == 1`，着色器误认为实体属于 Pass 1 的有效批次 0，因此 `tagged1` 为真。
3. **越界访问触发 GPU TDR**：
   在第二轮循环（`blk == 1`）中，代码拿着未初始化的阴影批布局（或越界的切片）强行执行了原子计数与 ICB / L2W / Cmds / Mtl 写入，引发显存越界访问，最终导致 GPU 挂起和设备丢失（`VK_ERROR_DEVICE_LOST`）。

---

## 4. 修复方案

利用 `CullPushConstants` 原本因 8 字节对齐预留的 4 字节填充空间，将主帧和阴影的批数彻底拆分：

### 4.1 C++ 侧扩展 PushConstant
在 `GPUCullSystem.cpp` 中定义双 Pass 批数：
```cpp
struct CullPushConstants
{
    // ... 11 × uint64_t 前置 ...
    uint32_t entity_count;
    uint32_t max_visible_count;
    uint32_t camera_row;
    uint32_t batch_count;         // 主帧批数 (batch_count_[0])
    uint32_t batch_block_stride;
    uint32_t batch_count_shadow;  // 阴影批数 (batch_count_[1])，利用原有 4B 填充
};
static_assert(sizeof(CullPushConstants) == 112, "CullPushConstants layout mismatch");
static_assert(offsetof(CullPushConstants, batch_count_shadow) == 108, "batch_count_shadow offset");
```
在派发前准确赋值：
```cpp
pc.batch_count        = batch_count_[0];
pc.batch_block_stride = kBatchBlockStride;
pc.batch_count_shadow = batch_count_[1];
```

### 4.2 GLSL 侧分 Pass 校验门禁
在 `entity_gpu_cull.glsl` 中更新布局声明，并分别使用各自 Pass 的有效批数进行校验：
```glsl
layout(push_constant) uniform PushConstants
{
    // ...
    uint   batch_count;         // 主帧批数
    uint   batch_block_stride;
    uint   batch_count_shadow;  // 阴影批数
} pc;

// 标签有效性判定严格隔离
const bool tagged0 = (batch0 != 0xFFFFFFFFu && batch0 < pc.batch_count);
const bool tagged1 = (batch1 != 0xFFFFFFFFu && batch1 < pc.batch_count_shadow);

if (!tagged0 && !tagged1)
    return;

for (uint blk = 0u; blk < ULRE_ARENA_PASSES; ++blk)
{
    if (blk == 0u && !cull_ok)
        continue;

    const uint cur_batch_count = (blk == 0u) ? pc.batch_count : pc.batch_count_shadow;
    const uint batch = (blk == 0u) ? batch0 : batch1;

    // 当前 Pass 批数为 0 或批号不匹配时，立即跳过当前扇区
    if (batch == 0xFFFFFFFFu || batch >= cur_batch_count)
        continue;

    // ... 后续安全写入 ...
}
```

---

## 5. 验证与结论

1. **测试结果**：
   - `DrawTriangle.cpp` 在启用 GPU Cull Dispatch 的状态下平稳运行，0 卡顿、0 超时、三角形正常渲染显示。
   - 全引擎现有走 CSM 级联阴影与离屏 Pass 的测试用例保持绿灯。
2. **架构准则**：
   - 当多个渲染阶段（Pass）共享同一 Compute 剔除派发以追求吞吐极致时，**数据输出空间（Buffer 扇区）和调度控制流（PushConstant / Active Mask）必须保持同等粒度的分块隔离**。
   - 任何一个 Pass 未开启时，必须能够在着色器入口处通过清晰的计数门禁（`count == 0`）零代价跳过，杜绝以单份主变量代表全场景状态。
