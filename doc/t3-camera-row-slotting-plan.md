# T3 · 相机行槽化（CameraInfo per-frame 行）工作方案

> 目标：让 `CameraInfo` 的每个相机数据按「帧槽」分份，使离屏 pass（阴影 prepass）写光源相机
> 不再覆写主帧在途的那一份；同时**不改变相机编号语义**、不引入 `CommitRow` 拒绝。
>
> 本文只定方案，不含实现。相关前置已提交：`e7525c10d`（per-frame 数据槽 = 当前 RT 的槽）。
>
> **订正（2026-09-28）**：本文 §1 表格里"光源相机常属**另一个世界**"是**错误归因**——光相机是
> `EnvironmentSystem` 直接 `make_shared` 创建、**不经 Entity/AddComponent 注册**，所以 `CollectCameras()`
> 看不到它；CSM 始终是"**一个世界 + 多个渲染过滤程**"（`EnvironmentSystem.cpp:216/389/421`）。
> 相机编号 / 存储 / 默认相机的**后续定稿**见 `doc/world-addresses-and-camera-model-plan.md`：
> 相机存储下沉**世界级**、0 号槽恒为本世界默认相机、世界容量 16 槽、三级解析（默认 → 最小实体号 → 强制 fallback）。

---

## 1. 为什么需要行槽化（问题定义）

`CameraInfo` 是**全局 SSBO 行池**，着色器通过 `pc_root.camera_row` 直接索引：

```glsl
// ShaderLibrary/ubo/scene_ubo.glsl:123
#define camera CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_row]
```

CPU 侧有两条写入者、两条读取者，**跨 pass 同时活跃**：

| 角色 | 位置 | 写入/读取的相机 |
|---|---|---|
| 主帧读取 | `GetActiveCameraID()` ← `Context::active_camera_id` | 主相机（`camera_id == 0`） |
| 离屏 pass 读取 | `RenderTo` 里 `active_camera_id = req.camera->camera_id` | 阴影光源相机（**本世界的系统内建相机**，不经 Entity 注册 ⇒ `CollectCameras()` 看不到） |
| 主帧写入 | `CameraSystem` 发布（本世界全部相机） | 主相机 |
| 离屏 pass 写入 | 同上 + `RenderTo` 按 `req.camera` 发布 | 光源相机 |

没有行槽化时，主帧与离屏 pass 对**同一行**读改写，且 GPU 侧可以重叠执行（timeline 车道只
保证 pass 之间的顺序，不保证同一行的读写不重叠）⇒ 典型症状是阴影按上一帧/半截的相机数据
生成（与 T10 的 L2W ring 覆写同源）。

**槽口径**（已随 `e7525c10d` 落地）：主帧槽 `[0,4)`，离屏 RT 槽带 `[4,8)`，`HGL_FRAME_SLOT_TOTAL = 8`，
两个方向天然不相交。行槽化要做的，是**把相机维度接到这个槽口径上**。

---

## 2. 约束（不变量）

1. **行必须 Active**：`ActiveRowPool::CommitRow` 校验 `IsActive(id)`（`src/Vulkan/buffer/ActiveRowPool.cpp:190-199`），
   行未申请则写入被拒、脏页不标记 ⇒ 相机数据到不了 GPU。**这是上一轮报错
   `CommitRow rejected invalid row: ssbo_id=5 id=4/12` 的全部原因。**
2. **编号语义不能动**：`camera_id == 0` 在仓库里是「主相机」的既成判据
   （`EnvironmentSystem.cpp:244` 释放门控、`CameraSystem.cpp:623` camera_ubo 门控、
   `Context.cpp:552/:640` 的兜底 `active_camera_id = 0`）⇒ 主相机必须仍然是 0，
   非主相机仍从 1 起递增。
3. **容量不预留增长**：相机上限 8（= 行池 64 行 ÷ 8 槽），超限 fail-fast。
4. **不许依赖行号连续性**：`ActiveRowPool::Acquire()` 优先复用 idle 列表（FIFO，`ActiveIDManager`），
   只有「无 idle」时才 `CreateActive` 从水位连续创建 ⇒ `Acquire` 出来的行号**不保证连续**。
5. **零兼容**：不做「有/无槽」双路径，不留旧单行 fallback。

---

## 3. 方案比较

| 方案 | 做法 | 结论 |
|---|---|---|
| **① 行带 Acquire**（已试，已撤） | `AcquireCamera` 连续 `Acquire` 8 行，行号 = 带首 + slot | ✗ 依赖 ①`Acquire` 的连续性（不成立，实测 base=1 而公式从 0 起算 ⇒ 错位）；②带首由池决定 ⇒ 相机号变成「行号」而不是「序号」，reserve=1 时首带从 1 起、与公式错位 ⇒ 数字与基线不符。 |
| **② 0-based 序号 + `reserve_rows=0`**（已试，已撤） | 预留给主相机的 0 号行让出去，序号 0-based | ✗ 相机编号相对基线整体左移一位，`camera_id==0` 假设被踩（会话中 ATS 出现异常，但受第 5 节读回故障干扰，**该归因未独立证实**）。 |
| **③ 整块预激活 + 纯算术行号**（推荐） | 池创建后把 64 行**全部置 Active**；相机「序号」由 registry 自己的位图分配（0 留给主相机，1..7 给其它相机）；行号 = `序号 × 8 + slot` | ✓ 行恒 Active（`CommitRow` 必然通过）；✓ 行号纯算术、与池的分配策略解耦；✓ 编号语义与基线**逐位一致**（主相机 0、其余 1..7）；✓ 不需要改 CMCore；✓ 释放/再申请不产生 id 空间碎片（位图复用，不碰池）。 |

**选 ③**。它是唯一同时满足 §2 全部约束、且不依赖池内部分配行为的方案。

---

## 4. 落地方案（③）

### 4.1 引擎侧（`ActiveRowPool`）

新增一个方法，语义限定为「固定布局行空间专用」：

```cpp
/// 把行号空间 [0, row_capacity) 全部置为 Active。
/// 用于行空间**完全由调用方静态划分**的池（如 CameraInfo：相机 × 帧槽）。
/// 目的：让 CommitRow 校验恒成立，且行号只由调用方的算术决定，不依赖 Acquire/Release。
bool ActiveRowPool::ActivateAllRows();
```

实现直接用现成 API（`CMCore/inc/hgl/type/ActiveIDManager.h:100`）：

```cpp
int *ids = ...;            // 栈上分块，避免大容量占栈
return ids_manager.CreateActive(buf, row_capacity) == int(row_capacity);
```

`ActiveRowPool::Create` 已重置行号空间，因此在 `Create` 之后调用即可。

### 4.2 注册表侧（`GlobalSSBOBufferRegistry`）

- 常量（已在现工作树中）：`kCameraInfoSlotCount = HGL_FRAME_SLOT_TOTAL`、
  `kCameraInfoRowCount = 64`（= 上限 8 相机 × 8 槽）、`kMaxCameraCount = 64/8` + `static_assert`。
- 配置表：`CameraInfo` 的 `reserve_rows` 用 **0**，并新增一个显式标记 `pre_activate_rows = true`
  （不要用「reserve==0 就自动预激活」这类隐式规则）。
  创建后若标记为真，调 `ActivateAllRows()`。
- **相机序号分配器**（registry 私有，不碰池）：

```cpp
uint32_t AcquireCamera()   // 返回相机序号；0 永远留给主相机，从 1 起找最小空闲
uint32_t ReleaseCamera(uint32_t camera_id)  // 序号 0 拒绝释放（与既有 camera_id != 0 门控一致）
```

  - 位图容量 `kMaxCameraCount`，耗尽 ⇒ `GLogError` + 返回 `InvalidRowID`（fail-fast，容量不扩容）。
  - 序号可复用（释放后回到空闲位图），**不产生池行号碎片**。
- 行号接口（唯一入口，杜绝各调用点自己算）：

```cpp
/// 行号 = camera_id * kCameraInfoSlotCount + slot；越界直接报错返回 false
bool WriteCameraRow(uint32_t camera_id, uint32_t slot, const CameraInfo &info);
```

### 4.3 消费侧

| 文件 | 改动 |
|---|---|
| `inc/hgl/ecs/core/Context.h` / `src/ecs/core/Context.cpp` | `GetActiveCameraRow()` = `active_camera_id * HGL_FRAME_SLOT_TOTAL + frame_index`；`PrepareRenderPassSetup` 里发布本世界相机；`RenderTo` 里按 `req.camera` 直接发布（光源相机是系统内建相机、不经 Entity 注册，`CollectCameras()` 看不到——这是上一轮实测到的真因；历史注释曾误记为"属另一个世界"） |
| `src/ecs/systems/tick/CameraSystem.{h,cpp}` | 新增 `PublishCamera(camera, slot)` / `PublishCameraRows(slot)`；删 tick 阶段写（槽此时未确定）；申请逻辑恢复基线语义：主相机 `camera_id = 0`（不申请，行已预激活）、非主相机在 `camera_id == 0` 时申请一次 |
| 4 个读取点 | `PrimitiveRenderSystem.cpp` / `PrimitiveOverlayRenderSystem.cpp` / `LineRenderPipeline.cpp` / `TextRenderPipeline.cpp` → 传 `GetActiveCameraRow()` |
| `ShaderLibrary/ubo/scene_ubo.glsl` | **不改**（`pc_root.camera_row` 收到的就是行号） |

### 4.4 契约与测试

- **新增运行时契约**：`ActiveRowPool` 暴露 `GetCommitRejectCount()`，测试与示例断言恒为 0。
  这条契约的价值已被证明——上一轮的错位 bug 全程没有任何测试报警，只有日志在刷。
- **改用例 13 的契约**：现有源文本契约检查 `ReleaseCamera(light_camera->camera_id)` 对称性
  （`TestCSMIncrementalPass.cpp:1373-1396`）。方案 ③ 下不再有「申请/释放池行」的泄漏语义
  （行恒占用），契约应改为断言：**序号分配/释放后位图可复用**、**序号 0 不可释放**、**超限 fail-fast**。
- **破坏验证（强制）**：把行号公式里的槽维度改成恒 0 ⇒ 必须复现「阴影串帧」或契约失败；
  不复现说明契约没有牙。

### 4.5 验收门

| 门 | 判据 |
|---|---|
| 构建 | rc=0 / 0 error，`purge-stale-deps.sh` 覆盖 `Context.h`、`CameraSystem.h`、`GlobalSSBOBufferRegistry.h`、`ActiveRowPool.h` |
| 单元/契约 | `TestCSMIncrementalPass` 21 Passed + 新契约通过 |
| 运行期 | `CascadeShadowMap`（`CSM_CACHE_DIFF=1 CSM_AUTOWALK=4`）多轮 `不一致=0`、0 VUID、**`CommitRow rejected` = 0** |
| 视觉 | `AlphaTestShadow` D1 契约 +（D3 见 §5） |

---

## 5. 已知干扰项：交换链颜色读回当前取不到画面

`AlphaTestShadow` 的 D3 契约（`receive_shadow` / `bias_multiplier` 旋钮）依赖**交换链颜色图**
帧外读回（PRESENT_SRC → TRANSFER_SRC）。本轮实测该路径当前返回**恒定图**：

- 三个变体转储逐字节相同（`cmp` 全等）；
- 转储仅 4 个唯一字节值且全非零 ⇒ 单色图，不是场景内容；
- 同一份源码（`(a)` 帧槽修正 alone）早前实测 D3 PASS、三帧数据各异。

⇒ 这是**独立于本次改动**的读回/环境问题，D1（离屏读回路径）不受影响、照常 PASS。

**结论**：在它修好之前，§4.5 的「视觉门」只能以 D1 为准；不要用 D3 的红/绿给相机行槽化定生死
（本轮已经吃过一次误判）。建议单列一条：排查 `VKSwapchainRenderTarget` 颜色图读回的布局/acquire 时序。

---

## 6. 执行顺序

1. 修/绕开交换链读回（§5），恢复 D3 可用；若暂不修，把 D3 标为不可用并只跑其余门。
2. `ActiveRowPool::ActivateAllRows()` + 配置表标记 + registry 序号分配器/`WriteCameraRow`。
3. `CameraSystem` 发布侧 + `Context` 行号/两处发布 + 4 个读取点。
4. `GetCommitRejectCount()` 契约 + 改用例 13 契约 + 破坏验证。
5. 跑 §4.5 全部门，绿后按批次提交（本方案天然两批：引擎侧「行空间预激活」与主体「相机行槽化」）。
