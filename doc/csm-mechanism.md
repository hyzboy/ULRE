# CSM（级联阴影贴图）工作机制 — 会话快速进入文档

> 目的：让新会话在最短时间内掌握 CSM 的数据流、不变量、开关、判据与改动禁忌。
> 相关文档：`doc/csm-readback-and-leak-followup.md`（遗留两项线索）。
> 相机行槽化（T3）与 Scene UBO 的 BDA 化（S1–S3）均已落地，现行口径见 `doc/world-addresses-and-camera-model-plan.md`。

## 1. 一句话结构

`EnvironmentSystem`（ECS 系统）持有 `graph::CascadedShadowController`（CSM 数学 + 滚动缓存），
每帧 `RenderMainLightShadowPass(main_camera, dt)` 逐级联产出「是否需要重绘 / 脏矩形 / 光空间矩阵 /
偏移」，渲染进各自的级联 RT（`kMaxShadowCascades = 4`），最后把每级联的 `ShadowCascadeInfo`
写进 `ShadowInfo` UBO，主帧着色器用 `pcf_shadow.glsl` 采样。

- 控制器：`inc/hgl/graph/render/lighting/CascadedShadowController.h`
- ECS 门面：`inc/hgl/ecs/systems/render/EnvironmentSystem.h`（实现 `.cpp` 同目录）
- 数据契约：`inc/hgl/graph/ubo/ShadowInfo.h`（`kMaxShadowCascades = 4`）
- GLSL：`ShaderLibrary/shadow/pcf_shadow.glsl`（选级 `EvalCascadeChain`）、`ShaderLibrary/ubo/scene_ubo.glsl`（`ShadowInfo`）
- 示例：`example/Shadow/CascadeShadowMap.cpp`（诊断/契约最全）、`example/Shadow/AlphaTestShadow.cpp`（D1/D3/D4 契约）
- 测试：`TestCSMIncrementalPass`（`TestRenderItemDataStorage` 已于 2026-10-14 随 4-ID 池表整族删除）

## 2. ECS 侧接口（谁调什么）

`EnvironmentSystem`（`EnvironmentSystem.h`）：

| 接口 | 作用 |
|---|---|
| `EnableMainLightShadow(config, shadow_map_size=1024)` / `DisableMainLightShadow()` / `IsMainLightShadowEnabled()` | 开关主光阴影，按配置建级联 RT |
| `RenderMainLightShadowPass(CameraComponent*, float dt)` | 每帧驱动控制器：算矩阵/脏矩形，产出各级联更新结果 |
| `GetShadowController()` | 拿到控制器（S5 统计、缓存状态、手工失效） |
| `GetCascadeRenderTarget(c)` / `GetCascadeDepthRange(c)` | 级联 RT 与深度区间（示例对拍用） |
| `SetCascadeMask/GetCascadeMask/SetCascadeEnabled/IsCascadeEnabled` | 逐级屏蔽（D5 单级对拍用） |
| `InvalidateMainLightStaticShadowCache()` | 让静态级联缓存整体失效（场景/锚点失效时） |
| `SetShadowInfo(info)` / `EditShadowInfo()` / `GetShadowInfo()` | 写**本世界**影子数据的 CPU 权威（`ECSContext::shadow_info`）；**T6b/P4**：设备下发 = 世界私有缓冲 + 提交点 `ECSContext::CommitShadowInfo`（写本世界帧 RT 的当前槽）；~~`MarkShadowDirty()`~~ 与 `immediate` 参数已删（影子数据不再挂 env profile） |

## 3. 数据契约（GPU 侧，`ShadowCascadeInfo`，176B，std140 ABI 锚点）

| 字段 | 含义 |
|---|---|
| `shadow_vp` | 该级联光空间 VP（读侧 `shadow_vp = light_proj * light_view`） |
| `shadow_params` | `x=bias, y=pcf_radius, z=darkness, w=normal-offset 强度(世界米, 0=关)` |
| `shadow_map_size` / `inv_shadow_map_size` | 贴图尺寸与其倒数 |
| `shadow_tex` | 该级联贴图的 bindless 纹理索引（`Vector4u`） |
| `cascade_params` | `x=split near, y=split far`；`z=静态链末级/动态层边界淡出带宽(占本级深度区间比例)`；`w=相邻级联交界带宽度(世界米, 0=硬切换)` |
| `cache_origin` | `x/y=光空间已 snap 的缓存原点，z=每纹素世界尺寸` |
| `cache_offset` | `x/y=物理贴图环形偏移(texel)` |
| `cache_valid_rect` | `x/y=有效区左上角，z/w=有效区尺寸(texel)` |

**🔒 关键不变量（曾被两次修过，别改回去）**：着色器选级**只**看 `cascade_params.x/y`；
被屏蔽的级联在它自己的区间里**返回受光**，**绝不允许下沉到更远的级联**
（`pcf_shadow.glsl::EvalCascadeChain`）。历史提交：`fix(shader): strictly isolate cascade selection by view_depth…`、
`fix(shadow): prevent cascade selection from sinking to farther cascade when masked`。

## 4. 控制器侧：滚动缓存与整级重建

`CascadedShadowController.h` 三个结构：

- `ShadowDirtyRect{x,y,width,height}` —— 条带脏矩形。
- `CascadeUpdateResult`（trivially copyable）：`need_full_update`（级联0/首帧/锚点失效/**位移不是整步滚动**）、
  `is_static_cache`（中远景静态滚动缓存）、`light_view`(未偏移)/`light_proj`/`light_view_draw`、
  `cache_offset`、`texel_world_size`、`sphere_radius`、`depth_range`、`resolved_bias`、脏矩形列表。
- `CascadeUpdateStats{full_update, cache_hit, strip_count, strip_texels, map_texels, offset}`
  —— **S5/D5 的代价来源**（条带 texel ÷ 整图 texel）。

`CascadedShadowConfig`（默认值即当前约定）：`cascade_count=4`、`split_lambda=0.85`、
`split_distances={15,45,100,250}`、`use_custom_splits=true`、`max_distance=250`、
`shadow_map_size=1024`、`caster_depth_margin=100`、`bias=0.002`、`per_cascade_bias_scale[4]={1,1,1,1}`、
`normal_offset_world`、`pcf_radius=1.5`、`darkness=0.12`、`c0_dynamic_overlay=false`
（为真时 CSM0 仅作近景动态层，静态阴影由 CSM1..N 从 near_z 起覆盖）。

### 静态/动态分离（用户已裁定，勿翻案）

- 世界按 `Mobility` 分类：`Static` 物体进静态级联缓存，`Movable` 走动态。
- `example/Shadow/CascadeShadowMap.cpp` 的 `InfiniteGround` 保持 **Static**，按**相机网格吸附**；
  跨格时**接受整级静态级联重建**并打一次性告警（这是明确拍板的行为，不是缺陷）。
- 手动整体失效用 `InvalidateMainLightStaticShadowCache()`。

### D5 横向锚定步长（用户拍板「B 档」= texel 口径）

- 锚定步长的**主参数是 shadowmap 侧 texel 数 `B_c`**，世界米数是派生量。
- 实测表：`B={0,16,16,32}`（逐级联）；代价 `loss = 1.416·B/(M−1.416·B)`，M=1024：
  B=8→1.12%、16→2.26%、32→4.63%、128→15.6%(1/8)、256→54.8%。
- **fail-safe**：`B ≥ M/1.416`（≈723）时退化 ⇒ 回退为「禁用锚定」而不是继续加大。
- `need_full_update` 的触发条件之一就是「位移不是整步滚动」——改步长策略必须同时想清楚这一点。

## 5. 逐帧时序与槽位（改前必读）

- per-frame 槽空间：**主帧 `[0,4)`、离屏 4..7、上限 8**（`inc/hgl/common/RenderOptions.h` 的 `HGL_FRAME_SLOT_TOTAL`）。
- **不变量：任何 per-frame ring 深度必须 == 槽空间**（`HGL_L2W_RING_FRAMES` 就等于它）。
  历史上 ring=3 时离屏槽与主帧槽别名 ⇒ prepass 覆写主帧在途 L2W ⇒ 整帧只剩清屏色（间歇）。
- 相机行池：**已是世界私有**（C1-3 落地，C1-4 补完模型）：`CameraInfoStorage` 128 行 = 16 相机槽 × 8 帧槽，
  行号 = `camera_slot * 8 + slot`，地址在世界表 `WorldAddresses::addr_camera_info`。
  **相机解析已是三级**（C1-4）：① 0 号槽的默认相机 → ② 显式 `is_main_camera` → ③ 最小 `EntityID` 的相机
  → ④ 都没有 ⇒ `(0,0,0)` 常驻 fallback（占 0 号槽、不进组件表）。槽号未分配用
  `CameraComponent::kInvalidSlot`（**不是 0**：0 = 默认相机专属槽）。
  定稿见 `doc/world-addresses-and-camera-model-plan.md`。
- 相机行发布按 `req.camera` 直发：阴影光源相机是**系统内建相机**（不经 Entity 注册 ⇒
  `CollectCameras()` 看不到），**不是**"属于另一个世界"（历史注释错误归因，已订正）。
- 相机数据走 **BDA**：`scene_ubo.glsl` 的 `camera` 宏 =
  `CameraInfoBufferRef(world_addresses.addr_camera_info).cameras[pc_root.camera_row]`
  （相机行表是**世界私有**；全局表里那两个旧字段 `addr_camera_info` 已随 C1-3 删除）。
  同一世界表内还带相机行表 / 实体行表 / 实体 4-ID 表 / L2W / 光源行 / 影子数据 / 环境光块等世界私有列
  （原 `addr_global_render_items` / `addr_draw_item_ids` 两个 4-ID 相关地址已于 2026-10-14 随池与二级索引表整族删除）。

- **视图矩阵必须由 `forward` 构造，禁止用 `target`**（`CameraSystem::UpdateMatrices` 非 custom 分支）：
  `target` 由 `UpdateTransform` 维护，当 `position` 被**外部直接写**（示例 autowalk `position.x += speed*delta`、
  任何在 tick 之外写位置的路径）时它会慢一拍 ⇒ `LookAtMatrix(position, 慢一拍的 target, up)` 算出**方向差约 30°**
  的视图，**该帧整幅画面渲成另一个机位**（实测同一姿态下 `viewT` 从 ~1.2m 跳到 ~13.5m；配合 autowalk 的
  `delta` 突变 ⇒ "隔几秒画面拉扯一次 / 刚出场抖"）。`forward` 与 `camera_data->viewDirection`、shader 的
  `view_line` 同源（`UpdateBasis` 由 yaw/pitch 得出），与 `position` 永远同帧一致；LookAt 模式方向仍来自
  `target` ⇒ 等价。
- **定位"某一帧视角不对"的可用手法**（需要时临时加打印，用完即删）：① 在解算处打印
  `camera_info->view[3]`（平移量）+ `pos/target/forward` —— 同一姿态下 `viewT` 不一致即解算输入错
  （本次即 1.2m ↔ 13.5m 横跳）；② 外围做逐帧颜色转储 + 亮度剖面**水平互相关**求逐帧位移：正常应
  ≈0~1px 单向平滑，出现 **±6~25px 符号交替**就是视角跳变（拉扯）。教训：`camera_row` / viewport 正确
  **不等于**行内矩阵正确——要直接看矩阵。

## 6. 开关与诊断（环境变量）

| 变量 | 作用 |
|---|---|
| `CSM_CACHE_DIFF=1` | S5 整级 vs 条带 **深度图对拍**，逐轮打印 `不一致=`（判据：恒 `不一致=0`） |
| `CSM_AUTOWALK=<m/s>` | 相机自动行走速度（**不是帧数**） |
| `CSM_CACHE_DIFF_FREEZE` | 冻结对拍基准（配合上一项） |
| `channels.shadow = true` | **S6 仪器**（默认静默）：逐 pass 打印 `[S6-PASS] cascade/kind=full|strip/mobility/off/rect`，与 |
| | `[S6-COLLECT]`（图元数 + Σ(entity<<16\|gen) 校验和）配对读 ⇒ "同批 caster 却在光栅化侧不同"才成立 |
| `ATS_SELFCHECK=1` | AlphaTestShadow 自检：D1/D3/D4 契约 + selfcheck 退出码 |
| `[perf] prepass_interval = N` | **prepass 性能账目**（每 N 个主帧报一行均值）：离屏 pass 次数/帧、总墙钟、
| | 两处 fence 等待（上沿等主帧 / 下沿等自身）、begin/draw/submit 细分。未设 = **零成本**
| | （`Context.cpp` 内匿名 namespace 门控）。基线数据见 §8.5；**引擎无 GPU 时间戳 ⇒ 这是 CPU 可见账** |

示例契约（AlphaTestShadow）：**D1** 填充率（`bbox=112x58 填充率 57.6%`）、
**D3** 外观变化（`receive_shadow 18189 px` / `bias_multiplier 600662 px`，注意亮度是低 3 字节**代理**口径）、
**D4** 行池写入（`CommitRow 拒绝 0`）。

## 7. 验证流程（改 CSM 后必须逐项跑）

1. **清陈旧产物缓存**：`rm -rf build/cache-hot/shader-cache`（不清会拿旧产物冒充新结果）。
2. 改到**头文件/结构大小**时先 purge：`purge-stale-deps.sh D:/ULRE D:/ULRE/build D:/ULRE/src D:/ULRE/inc D:/ULRE/example -- <headers>`。
3. 构建 → **禁用 `| grep error` 判结果**（吞错后会跑旧 exe ⇒ 假绿；构建失败时 MSBuild 不重链 exe）。
4. `ShaderResourceSchemaRegressionGate`：基线 **45 PASS / 0 FAIL**（每新增一个 shader 模块或一条用例 +1；含两张表各一条 parity + 各一条 `*-field-ownership` 归属用例）。
5. `TestCSMIncrementalPass` = **42 Passed**（末条 = Test 43；Test 42 = 机制层 **18 条契约**〔M1/M3/M4/M6/M7/M8/M9/M11/M12〕、禁复活 4 条；Test 43 = 离屏槽带生命周期账 a–f + M10/M11）；`TestRenderItemDataStorage` 通过。
6. `ATS_SELFCHECK=1 AlphaTestShadow` → 三契约 + selfcheck PASS + **0 VUID** + 数字与基线一致。
7. `CSM_CACHE_DIFF=1 CSM_AUTOWALK=4 CascadeShadowMap` → 多轮 **不一致=0**（该项无自动退出，需 timeout）。
8. **破坏验证**（证明门有牙）：如跳过行池预激活应看到 `D4 FAIL（拒绝 160 次）`、ATS rc=1。

## 8. 已知问题 / 遗留（都不是本次引入）

0. **反复 Enable / 运行中 Disable 灯影**（C3 探针暴露的生命周期缺口；**① 已于 2026-10-07 根治，`d9b2c6886`**）：
   - ~~同一会话里 `EnableMainLightShadow` 第二次以上 ⇒ `[RenderTargetManager] CSM_Cascade_0: in-flight 槽带越界（起点=8 槽数=1 上限=8）`~~ **已修**：离屏数据槽带改为占用位图 first-fit 分配 + `Destroy` 归还（`OffscreenSlotBands`，Test 43 生命周期账 + 端到端探针二次 Enable first=1 second=1 验证；根因与修法详见 `doc/future/entity-rows-gpu-cull-plan.md` §11.17）。
   - 世界存活时 `DisableMainLightShadow` ⇒ 2 条 `vkDestroySemaphore(): VkSemaphore[CSM_Cascade_0:Lane] that is currently in use by VkQueue`（级联 RT 的 timeline 车道在队列仍在用时销毁）。**已根治（2026-10-07，`5a085a627`）**：lane 有**外部**等待者（主帧提交 `AddFrameLaneWait` await 它），RT 自己枚举不到 ⇒ `RenderTargetData::Clear()` 在销毁同步对象前 `device->WaitIdle()`。「运行中 Disable」探针（第 120 帧）实测告警 **2 → 0**；契约 §9 M11。
   相机槽一侧（C3）在两条路径上都是干净对称的（申请/归还同槽，见 §7 第 5 项与 `doc/world-addresses-and-camera-model-plan.md` §2）。

0-bis. **离屏帧槽带的容量账（2026-10-07 起，V7 台架实测）**：级联 RT 数 = `CascadedShadowConfig::cascade_count`
   （此前一律固定建 4 张；`EnableMainLightShadow` 已改为按配置创建），**每张占 1 条离屏帧槽带**
   （`OffscreenSlotBands`；全设备共 `HGL_FRAME_SLOT_TOTAL - HGL_FRAME_SLOT_MAIN` = **4 条**，
   `RenderTargetManager::Create` first-fit 分配、`Destroy` 归还）⇒ 同设备可共存的离屏 RT 总额 ≤ 4。
   实测：`DualWorldShadows`（两世界各自开 CSM）用 3 条 = 主世界 1 级联 + 子世界 RT 1 + 子世界 1 级联；
   而"某世界 4 级联 + 任何第二个离屏 RT"必然分配失败（fail-fast，不静默降级）。要更多并发离屏 RT 只有提高
   `HGL_FRAME_SLOT_TOTAL`（当前不做：不按极端场景预留容量）。

1. **交换链颜色图帧外读回** —— **✅ 已闭合（2026-10-07 复核）**：所需的引擎侧「渲染后 present 前」回读窗口 + 非 acquire 态 fail-fast 护栏**均已落地**
   （`IRenderTarget::IsColorReadbackWindowOpen()` + 交换链 RT 的帧内回读钩子 + `ReadbackColorTarget` 在窗口外直接拒绝），
   各示例的帧摘要自检（`*_SELFCHECK`）正是走这条窗口。以下原文保留作背景：
   读回发生在帧后，而 acquire/present 在渲染阶段内开合 ⇒ 需引擎侧「渲染后 present 前」回读窗口 + 非 acquire 态 fail-fast 护栏。详见 `doc/csm-readback-and-leak-followup.md`。
2. **`[LEAK]` 报告不可观察**：示例无自动退出开关（`CSM_AUTOWALK` 是速度），headless 只会被 timeout 杀掉
   ⇒ 退出期报告没机会打印。需先加帧上限开关，再用引擎已有的按类型汇总（`src/Vulkan/VKDevice.cpp:205-227`）分类。
3. **ViewportInfo 每帧多份化**未做（与 BDA 化任务有耦合）。
4. ~~Scene 集剩余 5 个 UBO 待彻底 BDA 化~~ —— **✅ 已闭合（2026-10-07 复核）**：描述符机制**整体退役**（终态 = 唯一集 `pc_root` + `Bindless=0`），
   `inc/hgl/common/DescriptorSetTypeDef.h` 的注已明写「Set 0（Scene 集）与其绑定枚举**已整体退场**」⇒ 不再有待迁出的 UBO。

5. **prepass 性能账目（D7 的 T8）✅ 已量测（`f8760056b`，2026-10-07）** —— 探针 `[perf] prepass_interval = N`（见 §6）。
   实测床 = `CascadeShadowMap` 1600×900、稳态 60fps（帧 16.67ms）、50 个窗口 = 1500 帧：
   - **离屏 pass = 1.00 次/帧**（只有级联 0 每帧全量；其余 3 级走静态缓存 + dirty-rect 条带，稳态无条带）；
   - 合计均值 **3.76ms/帧**（中位 3.23）⇒ 占主帧 **≈19%**；
   - 细分（均值/中位）：**begin 2.32/2.31ms**（= `BeginManagedRenderFrame`，含 `PrepareRenderPassSetup` 的
     collect/批装配/剔除派发；极稳 2.16–2.59）、draw 0.23ms、**submit 0.047ms（43–57µs）**、
     上沿等主帧 0.83/0.27ms（**突发**：最大 4.39ms，50 窗口里 17 个 >0.5ms）、下沿等自身 0.23ms；
   - 预热窗口（级联全量重建、1.30 pass/帧）：begin 11.7ms、合计 13.9ms/帧 = 34ms 帧的 40.7%。
   ⇒ **结论：A6「4 级联合并成单 cmd / 单 prepass」不成立（不只是收益低）——已关闭（用户裁定）**：
   ① 每级更新节奏独立（`need_full_update` 逐级触发；中远景 = 静态滚动缓存 + 条带脏矩形）⇒ 同一帧里各级
   「要不要渲、渲多少」不同，**没有「每帧 4 次 submit」的稳态形态**（实测 1.00 次/帧）；
   ② 各级 `mobility_filter` 不同（级联0 = Movable、1..3 = Static）⇒ 没有「一份共享 prepass」可合并，
   合并要么遍历两遍、要么推翻「静态/动态分离」（已裁定勿翻案）；
   ③ strip 是**局部写**（scissor），合并会把条带变整图重建 ⇒ 放大写带宽，把 D5 省下的还回去。
   能合并的只有 submit（47µs）与部分等待；**`PrepareRenderPassSetup`（2.31ms）按级联不可省**
   （光矩阵 / caster 集 / scissor 各级都不同）⇒ **真正的优化线是「per-pass 装配」与「上沿等待突发」，
   不是「合并级联」**。要 prepass 的 **GPU** 时间须另立时间戳设施（全仓无 `vkCmdWriteTimestamp`/`VkQueryPool`）。

## 9. 改动禁忌（踩过的坑）

- **生成物禁止手改**：`ShaderLibrary/common/descriptor_macros.glsl` 由 `DescriptorMacroGen` 依真源
  （`DescriptorSetTypeDef.h` 枚举 + `kDescriptorBindingMacros` 表）生成，手改会让内容哈希契约红、
  产物缓存串味（表现为「语义与名字/结构错配」「资源凭空掉行」）。改真源 → `--emit` → `--verify`。
- **并行有序表是位置耦合**：`names[] ↔ semantic_values[]`、目录行 ↔ 枚举，删中间项会静默错位；必须成对删改。
- **别用行号定位补丁目标**（编辑途中文件会偏移）；本仓 CRLF，多行补丁易失配，优先单行锚点或带断言的字节级替换。
- 提交：`git add -u -- <dirs>` 后**不带 pathspec** 提交（`git commit -F msg -- <paths>` 会漏文件）；
  原生 git 读不到 `/tmp` 路径；`index.lock` 偶发竞争，清锁重试。
