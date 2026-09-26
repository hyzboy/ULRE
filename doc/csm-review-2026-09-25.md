# CSM 实现复核：Code Review + Tech Review

> 复核时间 2026-09-25；范围 = `2026-09-23 08:00 → HEAD(61a196c91)` 分支 `CSM` 上的阴影相关提交。
> 结论前置：主体设计自洽且契约测试实测全绿；最高优先级问题 = 阴影 pass 从未请求专用 ShadowCaster 程序变体（性能 + attachment feedback loop）。
> 本文只做复核，未改动任何源码。

---

## 0. 复核范围与实测证据

### 0.1 提交范围

CSM 主线（约 30 个提交，其中 20 个只动 CSM 相关文件）：

| 提交 | 内容 |
|------|------|
| `2987e5bfa` | ShadowInfo 环形结构扩展（cascades / cache_origin / cache_offset / cache_valid_rect） |
| `6c7b6344a` | `CascadedShadowController` 首版 + `pcf_shadow.glsl` 环形寻址 |
| `4cfc57501` / `3e1bf254b` | 示例 `example/Basic/CascadeShadowMap.cpp` + 文档 |
| `a6c5ce3ae` | 两处几何根因：`light_forward` 二次取反；`zfar` 缺接收端余量 |
| `f09d2d8f9` | `IRenderTarget` 自持视口（离屏 RT 不再篡改主视口） |
| `b0b850109` | `ShadowInfo` 被在途帧覆写 → 分槽 `kShadowUboRing` |
| `fbe0f402b` | CSM pipeline 收敛：`CullMode` 显式化、`RefreshCameraInfo*` 拆分、契约测试扩到 892 行 |
| `37157d8e9` | 法线偏移（normal-offset）+ `bias_world` 世界单位 bias |
| `3cc91da20` / `6618554c0` | 背面渲染 + bias 极性；Poisson PCF 开关 |
| `384b36a4b` / `84e4925ca` / `8c61efb1f` | 动静分层 + 覆盖架构重构；近距混合修正；CSM0/1 同距 |
| `7563ccfa1` / `a5c66e8b3` / `f923a9dc5` | `ShadowComponent`；阴影 pass intent + caster 距离剔除；`EnvironmentSystem` 托管 |
| `4fad17386` / `9636ead7e` / `c3698a734` | `ScenePipelineMode`；`RenderMainLightShadowPass` 黄金路径；测试自动化 |
| `9c70f13e8` → `f1d30c88c` | 级联掩码；Poisson 半径/交界带调整；选级硬规则收敛 |

### 0.2 实测 1 —— 契约测试（真实产物运行）

```
cmake --build build --config Debug --target TestCSMIncrementalPass   # exit 0
./build/out/Windows_64_Debug/TestCSMIncrementalPass.exe              # EXIT=0（从仓库根运行）

[CSM-CACHE]     frames=600 full=[600,33,32,34] band=[0,0,0,0]
[CSM-COHERENCE] frames=400 hits=[377,377,380] max_delta=0.000000 (c=1 f=140) fail=0
[CSM-COVERAGE]  lateral_step=10m frames=128 fail=0 worst_ndc=[0.000000,0.719213,0.747521,0.738003]
[CSM-SPIN]      frames=240 hits=347 max_delta=0.000000 max_radius_delta=0.000061m fail=0
[CSM-BIAS]      default normalized=[-0.003 -0.003 -0.003 -0.003] depth_range=[346 349 619 953]m
[CSM-BIAS]      per-cascade scale=[1 2 3 0.5] -> normalized=[-0.002 -0.004 -0.006 -0.001]
[CSM-BIAS]      bias_world=-1.15m world=[-1.1500..-1.1500]m (constant)
[CSM-NORMAL-OFFSET] default strength=0.00m / configured strength=0.35m on [0.350000 ×4]
Test 1..10 全部 Passed（含 6D、7B、7C、8、9、10），无 Failed
```

### 0.3 实测 2 —— 示例全流程模板路由（`CascadeShadowMap.exe`，约 20s）

```
[CSM] shadow bias_world=-0.20m normal_offset=0.10m
[EnvironmentSystem] Main light shadow enabled successfully (4 cascades, size=1024)

SkyRoute 行数 = 101
depth=1 的行数 = 0
template 统计 = 101 × forward_lit_shadowed_ao
shadow_caster / depth_only 模板出现次数 = 0
VUID / [ERROR] = 0（本机未启用 VK_LAYER_KHRONOS_validation）
```

复现方式见 §5。

---

## 1. Code Review

### A1【高】阴影 pass 用的是前向着色程序，专用 ShadowCaster 路径从未被启用

证据链（全部来自全仓 grep，非推测）：

| 环节 | 位置 | 事实 |
|------|------|------|
| purpose 唯一来源 | `inc/hgl/ecs/components/PrimitiveComponent.h:107-108`、`:166-177` | 默认 `PrimitiveVariantPurpose::Surface`；`SetPrimitiveVariantPurpose()` **全仓零调用者**（仅定义处与 ShaderGen 回归测试出现） |
| purpose → 程序 purpose | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:445-456` | `Surface → ShaderProgramPurpose::ForwardColor` |
| 请求构造 | 同上 `:499` | `mtl_request.shader_program_purpose = effective_purpose`（全仓唯一构造点，**不看 RT 是否 depth-only**） |
| 模板分派 | `src/SceneGraph/module/ShaderProgramManager.cpp:217-230`、`src/ShaderGen/contract/MaterialOutputContract.cpp:36-46` | 只有 `DepthOnly`/`ShadowDepth` 才走 `ShadowCasterOpaque` / `ShadowCasterMasked` |
| 专用实现已就绪 | `src/ShaderGen/template/FragmentTemplateComposer.cpp:659-661` | `ComposeShadow()`：空 fragment main、无 material/sky descriptors、裁掉无关 varying |
| 实测 | §0.3 | 101/101 `depth=0`，`forward_lit_shadowed_ao` |

**后果**（按可信度分层）：

1. 【已证】4 级联深度图用 `forward_lit_shadowed_ao` 片元着色器绘制：PBR 材质采样 + 16 tap Poisson PCF + 天空/环境光全部执行，颜色写入不存在的附件后被丢弃。纯浪费，且**每次缓存 miss 都要付**——直接抵消"静态级联 0 DrawCall"的性能收益。
2. 【推论，建议 RenderDoc / validation 确认】`ShaderLibrary/shadow/pcf_shadow.glsl:156` 读 `shadow.cascades[c].shadow_tex.x` 并采样；渲染级联 c 时该句柄就是**当前 pass 的 depth attachment**（`src/ecs/systems/render/EnvironmentSystem.cpp:210-211` 注册的正是同一张 RT 的深度纹理）→ 同一 image 既作 attachment 又被采样，布局为 `DEPTH_ATTACHMENT_OPTIMAL` 而非可采样布局：属 Vulkan attachment feedback loop（UB）。本机验证层未开所以静默通过。
3. 【设计层面】MaterialCoverageContract 为 shadow/depth 专门裁掉的资源绑定、alpha 覆盖契约分派等优化全部拿不到。当前 alpha test 恰好因 Surface 变体也带 `discard` 而未出错，属侥幸。

**修复（最小改动）**：在 collect 侧按 pass 覆盖 purpose，不必动 `PrimitiveVariantPurpose`（它还兼管几何/材质变体选择）：

```cpp
// src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:~445
if (world && world->IsCurrentPassShadow())
    effective_purpose = graph::mtl::ShaderProgramPurpose::ShadowDepth;   // → ShadowCasterOpaque/Masked
```

**复验**：开 `khronos.validation` 跑示例应 0 feedback VUID；RenderDoc 里 shadow pass 的 fragment shader 应为空 main。

### A2【中】每次 `EnableMainLightShadow()` 泄漏一个 CameraInfo 行

- `src/ecs/systems/tick/CameraSystem.cpp:689`：非主相机 `camera->camera_id = registry->AcquireCamera();`
- `src/ecs/systems/render/EnvironmentSystem.cpp:214`：每次 Enable 都 `make_shared<CameraComponent>("AutoCSMLightCamera")`
- `EnvironmentSystem.cpp:237`：Disable 只 `light_camera.reset()`，不归还行
- `ReleaseCamera` 存在（`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:278`）但**全仓零调用者**

与"容量不预留、超限 fail-fast"的既有约定冲突。修：记住 `light_camera->camera_id` 并在 Disable 时归还；顺带把 `CascadedShadowController` 的裸 `new`/`delete`（`EnvironmentSystem.cpp:184`/`:233`）换成 `unique_ptr`。

### A3【中】静态级联没有任何失效钩子

`CascadedShadowController::InvalidateStaticCache()`（`.h:151`、`.cpp:43-50`）**零调用者**；`Update()` 的失效条件只有三个（`.cpp:325-326`）：`valid==0`、`scene_revision` 变、沿光轴锚点跨步。因此运行时新增/删除静态物体、移动静态物体、切换其材质/贴图都不会刷新已缓存深度，最多滞后 16m（沿光轴锚定）或 2m（横向锚定）才整级重建，相机静止时**永不刷新**。

对"无运行时静态变更"的场景无害，但该前提既没写进文档，也没有接线点。建议：`EnvironmentSystem` 暴露转发 API + 在 `TransformSystem::SubmitTransformUpdates` 检出 static 物体 L2W 变更时按 revision 触发；或至少在文档把前提写死。

### A4【中】PCF 在贴图窗口边缘经 `fract()` 环绕到对侧取深度

- `ShaderLibrary/shadow/pcf_shadow.glsl:151-152`：`phys_uv = fract(shadow_uv + offset_uv)`
- `pcf_shadow.glsl:74-75`：逐 tap `fract()`（`wrap_uv = true`）
- `pcf_shadow.glsl:213`：`EvalCascadeShadowAt` 已把 uv clamp 到半纹素
- `CascadedShadowController.cpp:388`：`cache_offset` 恒为 `scroll_offset`，而它永远是 `(0,0,0,0)`

环形寻址（Toroidal clipmap）未启用，此处 `fract` 只剩副作用：半径 1.5 texel 的 taps 越过边界后**跳到贴图对侧**取深度 → 每个级联方框内约 2 texel 宽的一圈杂斑。修：环形寻址落地前改为 clamp（或按 `cache_offset` 是否非零选择 `wrap_uv`）。

### A5【低-中】`ShadowComponent` 四个旋钮有两个完全无消费者

- `receive_shadow`：`ShadowComponent.h:18/37`，`CanReceiveShadow()` 只被 `src/ecs/support/TestCSMIncrementalPass.cpp:1077/1101` 使用
- `bias_multiplier`：`ShadowComponent.h:19/41`，`RenderableComponent.h:104 GetShadowBiasMultiplier()` **零调用者**
- `max_cast_distance` 是唯一真正被消费的（`RenderPrimitiveCollectSystem.cpp:1172`/`:1293`）

而 `doc/shadow-component-and-automated-pipeline-design.md:19` 把两者写成已生效的"标准默认行为"，Test 8C 又断言了整条 getter 转发链——读出"已接通"的错觉。要么落地（receive 需接 shader/材质侧，bias_multiplier 需进 material row），要么删字段并把文档标为未接通。

### A6【低-中】一帧最多 4 次离屏 `RenderTo`，各带一次全流水线排空

- `src/ecs/systems/render/EnvironmentSystem.cpp:302-354`：逐级联发 `RenderTo`
- `src/ecs/core/Context.cpp:502`：每次开头 `saved_target->WaitFence()`；`inc/hgl/vk/VKRenderTargetSwapchain.h:26` 注明 `WaitFence()` = **full drain of all slots**
- `src/ecs/core/Context.cpp:590`：每次结尾 `rt->WaitFence()`
- 中间还有一整套 `BeginManagedRenderFrame(false)`：`RenderPreBeginFrame` / `SyncRenderTargetViewport` / `PrepareRenderPassSetup`（collect+batch+upload+sync）/ `BeginRenderPass` / `Submit`

缓存 miss 的一帧 = 最多 4 次 submit、4 次全槽排空、4 次 RT 等待，GPU/CPU 完全串行。副作用：`EnvironmentSystem` 自身注册在 `ExecutionPhase::RenderPreBeginFrame`（`.cpp:21`），其 Update 因此被跑 **5 次/帧**。

建议：4 级联录进同一 command buffer（4 个 dynamic-rendering 深度 pass，或 layered array + 单 pass），一次 submit；prepass 从"逐级联 RenderTo"改为"一次 prepass 调用"。

### A7【低】命名/文档与实现漂移

- `.ai/skills/SKILL_CASCADED_SHADOW_CSM.md:335/387/442` 写示例 `bias_world = -1.15f`，实际 `example/Basic/CascadeShadowMap.cpp:78` 是 `-0.20f`（`61a196c91` 只同步了 normal offset 0.10f）
- 示例窗口标题 `example/Basic/CascadeShadowMap.cpp:886` 仍为 "CSM Rolling Toroidal Cache & Mobility Stream"，而环形寻址未启用（SKILL §4 自己承认；`cache_offset`/`scroll_offset`/`cache_valid_rect` 恒为 `(0,0,0,0)` / `(0,0,W,H)`）
- 未消费 API：`CascadeUpdateResult::texel_shift_x/y`（只写不读）、`ShadowDirtyRect[4]` 池（永远只放一个全图矩形）、`GetCascadeRenderTarget()`、`InvalidateStaticCache()`

### A8【低】`RenderMainLightShadowPass` 的 scissor 增量分支是死代码

`EnvironmentSystem.cpp:330-353` 永远不会进入：控制器要么缓存命中（`dirty_rect_count==0`，什么都不做），要么 `need_full_update=true` 且带全图矩形（`CascadedShadowController.cpp:307/329/360`）。要么真正实现条带滚送（环形寻址 + 部分矩形），要么连同 `ShadowDirtyRect` 池一起删——保留永不执行的分支会误导后来人以为"滚动更新"已实现。

### A9【低】`zfar` 公式在两处手抄

`CascadedShadowController.cpp:230`（拟合时使用）与 `:289-293`（反推 `depth_range` 供 `bias_world` 换算）是同一公式的两份实现，文件顶部注释（`:9-11`）还专门强调"两处必须一致"。应由 `CalculateCascadeBounds` 直接 out 出 `zfar`/`depth_range`，`Update` 只消费——否则以后调 `caster_depth_margin` 语义时必然漏改一处，且漏改只表现为"半影/漏光"这类难归因现象。

### A10【低】`EnableMainLightShadow` 的边界与头文件重量

- `EnvironmentSystem.cpp:161-162`：重复 Enable 会整组销毁重建 4 张 D32F RT + 4 个 bindless 句柄 + 新相机（叠加 A2 的泄漏）
- `inc/hgl/ecs/systems/render/EnvironmentSystem.h:7-8`：为一个 `RenderTargetHandle` 成员把 `RenderTargetDesc.h` / `RenderTargetManager.h` 拉进公共头（可前向声明 + 成员下沉）

---

## 2. Tech Review

### 2.1 做对了的（后续重构勿丢）

| 项 | 评价 |
|----|------|
| 动静分层 + 双轴锚定 + 冻结矩阵 | 用"级联内容稳定"换"静态级联 0 DrawCall"，有硬指标：横向锚定后重绘 −92%/−86%/−66%；矩阵恒定性由 Test 5B-1/5C 实测 `max_delta=0.000000` 背书 |
| 选级与采样分离 | `EvalPCFShadowAt(worldPos, selectPos)` 双入参让法线偏移不污染级联归属，7C 源码契约守住该不变量 |
| `bias_world` 世界单位化 | 正视了"同一归一化 bias 在 2.75 倍深度范围差下不可调"的真问题；与 `per_cascade_bias_scale` 的优先级有 6A-6D 契约 |
| 法线偏移（`tan(θ)` 加权） | 补上引擎缺 `vkCmdSetDepthBias`（`VKPipelineResolver` 无 `VK_DYNAMIC_STATE_DEPTH_BIAS`）的短板；几何法线 / 光方向同源 / 总开关与强度分离三条约束写得很清楚 |
| 多帧在飞一致性 | `ShadowInfo` 分槽 + 只写当前槽 + `MarkDirty` 不写 GPU，并明确禁止"全槽 WaitFence 压症状"（`doc/shadow-ubo-inflight-overwrite.md`） |
| 屏蔽级联硬规则 | "先按 view_depth 定级，再判 `shadow_tex.x==0` 返回受光，绝不下沉"，连反例（关 CSM1 后近景被 CSM2 接管且无缝）都写清了 |
| `§3.4` 锚定基准 | `snapped_sphere_center` 必须回到 `cx0/cy0` 而非 `cx/cy`——这类方案的经典致命细节，有反证记录 |

### 2.2 结构性问题

| # | 问题 | 判断 |
|---|------|------|
| T1 | 静态滚动缓存的前提（"静态物体永不变"）引擎不保证，也没有失效链（见 A3） | 要么补钩子，要么做成显式开关并写清适用域（无运行时静态变更的地形/城市）；否则是必还的技术债 |
| T2 | 级联拟合用"包围球 + 正交方框"而非光空间 AABB | **有意识的交易**：换朝向无关半径（Test 5C 直接依赖），代价是纹素利用率（≈1.4–2× 边长浪费）。SKILL 应写明"为什么不收紧"，并把 AABB + 锚定补偿列为精度优化项 |
| T3 | 4 张独立 D32F RT vs atlas / depth array | 16MB + 4 个 bindless 槽 + 每 RT 一套 pipeline 键控（`PrimitiveComponent.h` 的 `resolvedRuntimePipelineMap`）；SKILL §10 已列，优先级合理 |
| T4 | 自动化 prepass 只在 `ECSContext::Render(dt, graph, pre_render)` 一条入口被驱动（`src/ecs/core/RenderGraph.cpp:171`） | 未来若新增/绕过帧入口，阴影会**静默不生效**（无断言无日志）。建议挂到 `BeginManagedRenderFrame` 并显式区分主帧/离屏帧，或入口缺失时告警 |
| T5 | `ScenePipelineMode` 五个预留空壳 | 目前只有 `StandardLitCSM` 有实现；零兼容偏好下空壳会退化成死分支。建议未实现模式在 switch 里至少 `GLogWarning`/断言，避免调用方误以为已被托管；`Custom` 的语义（不插入任何 prepass）应进文档 |
| T6 | CSM 塞进 `EnvironmentSystem` | 该系统原定位是"瘦转发层"（`.h:24-30`），现同时持有控制器、4 张 RT、bindless 句柄、光相机、掩码状态、pass 录制。建议抽 `MainLightShadowSystem` / `CSMShadowRuntime`，顺带用 RAII 成员一并解决 A2/A10 |
| T7 | 引擎已有 `OffscreenWorld`（`depth_only` + `cull_mode`）而 CSM 手搓 RT | 二者其实都对（阴影 pass 必须复用主世界实体，不能另起 ECSContext），但文档没写"为什么不用 `OffscreenWorld`"，未来极易被误"统一"。SKILL 应补一句设计约束 |
| T8 | 性能账目只有重绘次数，没有时间维度 | 建议先修 A1，再量测 prepass GPU 时间 / 4 次 submit 的 CPU 等待 / forward 变体多出来的片元开销；A1 修好后"0 DrawCall"的收益才真正兑现 |
| T9 | 契约测试里的恒真断言 | `TestCSMIncrementalPass.cpp:1107-1125`（8D）自己重算距离平方再和自己比，不调生产代码；`9A/9B` 只测 setter。SKILL §8 自己要求"写完断言必须故意破坏确认它会失败"，这几条不满足该标准——只能证明"存在"，不能守回归。建议改成对生产入口的断言（如经 `RenderTo(req)` 观察 `active_mobility_filter` / `IsCurrentPassShadow` 的实际效果，或让 collect 侧暴露 per-pass 计数） |
| T10 | 离屏 pass 写 ring 槽位的隐含不变量 | prepass 发生在 acquire 之前，`PrepareRenderPassSetup` 用的是"当前 RT"的 frame index（`Context.cpp:406` ← `RenderSystemCore.cpp:72`），离屏 RT 上即 0 → 会写 L2W ring 的槽 0。当前正确性完全依赖 `RenderTo` 开头的全槽排空（`Context.cpp:502`）。这条不变量应写进注释：一旦有人为了性能把全槽排空改成只等当前槽，就会静默踩坏在途帧 |

---

## 3. 建议执行顺序

1. **A1** 阴影 pass purpose 接线 → 拿回性能并消除 attachment feedback loop（连带开 validation 复验）
2. **A4** PCF 边缘 `fract` → clamp（环形寻址落地时再切回）
3. **A2 / A10 / T6** 资源所有权与托管拆分：light camera 归还相机行、controller 改 `unique_ptr`、Enable 幂等
4. **A3** 静态缓存失效钩子（或至少 API + 文档化的适用前提）
5. **A6** 4 级联合并到单 command buffer / 单次 prepass
6. **A9 / A8 / A7 的未消费 API** 一并清理（先删遗留再改进）
7. **A5** `receive_shadow` / `bias_multiplier`：实现或删
8. 文档同步（见 §4）

每个任务按既有惯例各自验证：改动控制器/矩阵 → `TestCSMIncrementalPass` 全绿（5A/5B-1/5B-2/5C）；改动 shader 的偏移代码 → 7A/7B/7C 全绿；改动渲染路径 → 示例冒烟 + 视觉确认。

---

## 4. 文档同步清单

- [ ] `.ai/skills/SKILL_CASCADED_SHADOW_CSM.md:335/387/442`：示例 `bias_world` → `-0.20f`（`example/Basic/CascadeShadowMap.cpp:78`）
- [ ] 同文件 §4：明确环形寻址未启用（`cache_offset` 恒 0）与 `fract` 的副作用（A4）
- [ ] 同文件补：`ShadowCaster/DepthOnly` 变体未被启用（A1）与静态缓存失效链缺失（A3）两个已知缺口
- [ ] 同文件补 T2 / T7：包围球拟合的交易、为什么不用 `OffscreenWorld`
- [ ] `example/Basic/CascadeShadowMap.cpp:886` 窗口标题去掉 "Toroidal Cache"（未实现）
- [ ] `doc/shadow-component-and-automated-pipeline-design.md:19`：标注 `receive_shadow` / `bias_multiplier` 尚未接通

---

## 4.5 执行结果附记（2026-09-26 更新）

§3 执行顺序的落地状态（提交号见 `git log`；取证方法与判读基准见
`doc/alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md`）：

| 项 | 状态 | 说明 |
|----|------|------|
| A1 阴影 pass 程序接线 | ✅ 完成 | MaterialComponent 双槽（forward/shadow_program 分离）；`shadow_caster_opaque` 路由 + validation 0 VUID |
| A4 PCF 边缘 fract | ✅ 完成 | `cache_offset` 非零自适应 wrap/clamp；Test 7C 第 11 条契约锁定 |
| A2 CameraInfo 行泄漏 | ✅ 完成 | Disable 归还 + 10D 源码契约；正反双取证（20 轮 Enable/Disable 行池稳定） |
| A3 静态缓存失效链 | ✅ 完成 | TransformSystem 检出 → static_scene_revision → EnvironmentSystem 消费；9D 行为契约；`InvalidateMainLightStaticShadowCache()` 转发 API |
| A1-4 masked caster 行 | ✅ 完成 | 过程中发现并修复 **depth-only FS 剥除**（终极根因）、pipeline program 键控、base_addr 断链、forward alpha test 未接线等 7 层问题——详见附记文档；深度图读回判读：棋盘投影填充 ~57%=镂空 |
| A5 / A6 / A7-A10 / T 系列 | ⬜ 待做 | 顺序不变 |
| 文档同步 | 🟨 部分 | SKILL_CASCADED_SHADOW_CSM 的 A1/A3/A4 条目已更新；本清单剩余项（bias_world 示例值、窗口标题、shadow-component 文档标注）待 A5 一并处理 |

新发现并已另案记录的问题：`SetLocalPosition` 等无同值短路（每帧重复 set 同值
会被 A3 链判为变更 → 静态级联每帧全量重绘；示例已修，引擎级短路待决策）。

A7 原文的两处状态修正（正文保留历史，不回改）：`InvalidateStaticCache()` 与
`GetCascadeRenderTarget()` **已有调用者**（A3 失效链接线 / AlphaTestShadow
深度读回），不再是未消费 API；`texel_shift_x/y` 已删除（环形寻址落地时随
条带机制一并重设计）。A9 的 zfar 双份公式已合一（`CalculateCascadeBounds`
输出 zfar，Update 只消费）；窗口标题已去掉 "Toroidal Cache"。

---

## 5. 复现方法

```bash
# 契约测试（从仓库根运行）
cmake --build build --config Debug --target TestCSMIncrementalPass
./build/out/Windows_64_Debug/TestCSMIncrementalPass.exe        # 期望末尾 EXIT=0，26 项全 Passed

# 示例模板路由取证（后台起，约 20s 后取证再杀）
cd /e/ULRE && ./build/out/Windows_64_Debug/CascadeShadowMap.exe > "$LOCALAPPDATA/Temp/csm_run.log" 2>&1 &
sleep 20
grep -ac "SkyRoute"                "$LOCALAPPDATA/Temp/csm_run.log"   # 101
grep -ac "depth=1"                 "$LOCALAPPDATA/Temp/csm_run.log"   # 0   ← A1 的关键证据
grep -ao "template=[a-z_]*" "$LOCALAPPDATA/Temp/csm_run.log" | sort | uniq -c
taskkill /F /IM CascadeShadowMap.exe
```

A1 修好后此处应出现新的模板（`shadow_caster_opaque`）、且 `depth=1` 计数非零。
