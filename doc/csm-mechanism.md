# CSM（级联阴影贴图）工作机制 — 会话快速进入文档

> 目的：让新会话在最短时间内掌握 CSM 的数据流、不变量、开关、判据与改动禁忌。
> 相关文档：`doc/csm-readback-and-leak-followup.md`（遗留两项线索）、
> `doc/scene-ubo-bda-migration-handoff.md`（BDA 化交接）、`doc/t3-camera-row-slotting-plan.md`（相机行槽化）。

## 1. 一句话结构

`EnvironmentSystem`（ECS 系统）持有 `graph::CascadedShadowController`（CSM 数学 + 滚动缓存），
每帧 `RenderMainLightShadowPass(main_camera, dt)` 逐级联产出「是否需要重绘 / 脏矩形 / 光空间矩阵 /
偏移」，渲染进各自的级联 RT（`kMaxShadowCascades = 4`），最后把每级联的 `ShadowCascadeInfo`
写进 `ShadowInfo` UBO，主帧着色器用 `pcf_shadow.glsl` 采样。

- 控制器：`inc/hgl/graph/render/lighting/CascadedShadowController.h`
- ECS 门面：`inc/hgl/ecs/systems/render/EnvironmentSystem.h`（实现 `.cpp` 同目录）
- 数据契约：`inc/hgl/graph/ubo/ShadowInfo.h`（`kMaxShadowCascades = 4`）
- GLSL：`ShaderLibrary/shadow/pcf_shadow.glsl`（选级 `EvalCascadeChain`）、`ShaderLibrary/ubo/scene_ubo.glsl`（`ShadowInfo`）
- 示例：`example/Basic/CascadeShadowMap.cpp`（诊断/契约最全）、`example/Basic/AlphaTestShadow.cpp`（D1/D3/D4 契约）
- 测试：`TestCSMIncrementalPass`（21 项）、`TestRenderItemDataStorage`

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
| `Set/GetShadowInfo(info, immediate)` / `MarkShadowDirty()` | ShadowInfo UBO 下发 |

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
- `example/Basic/CascadeShadowMap.cpp` 的 `InfiniteGround` 保持 **Static**，按**相机网格吸附**；
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
  同一世界表内还带 `addr_global_render_items` / `addr_draw_item_ids`（4-ID 解析用，当前未启用）。

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
| `ATS_SELFCHECK=1` | AlphaTestShadow 自检：D1/D3/D4 契约 + selfcheck 退出码 |

示例契约（AlphaTestShadow）：**D1** 填充率（`bbox=112x58 填充率 57.6%`）、
**D3** 外观变化（`receive_shadow 18189 px` / `bias_multiplier 600662 px`，注意亮度是低 3 字节**代理**口径）、
**D4** 行池写入（`CommitRow 拒绝 0`）。

## 7. 验证流程（改 CSM 后必须逐项跑）

1. **清陈旧产物缓存**：`rm -rf build/cache-hot/shader-cache`（不清会拿旧产物冒充新结果）。
2. 改到**头文件/结构大小**时先 purge：`purge-stale-deps.sh E:/ULRE E:/ULRE/build E:/ULRE/src E:/ULRE/inc E:/ULRE/example -- <headers>`。
3. 构建 → **禁用 `| grep error` 判结果**（吞错后会跑旧 exe ⇒ 假绿；构建失败时 MSBuild 不重链 exe）。
4. `ShaderResourceSchemaRegressionGate`：基线 **42 PASS / 0 FAIL**（含两张表各一条 parity + 各一条 `*-field-ownership` 归属用例，见 §6 契约 ④）。
5. `TestCSMIncrementalPass` = **22 Passed**（Test 22 = 两世界同帧隔离契约）；`TestRenderItemDataStorage` 通过。
6. `ATS_SELFCHECK=1 AlphaTestShadow` → 三契约 + selfcheck PASS + **0 VUID** + 数字与基线一致。
7. `CSM_CACHE_DIFF=1 CSM_AUTOWALK=4 CascadeShadowMap` → 多轮 **不一致=0**（该项无自动退出，需 timeout）。
8. **破坏验证**（证明门有牙）：如跳过行池预激活应看到 `D4 FAIL（拒绝 160 次）`、ATS rc=1。

## 8. 已知问题 / 遗留（都不是本次引入）

1. **交换链颜色图帧外读回**（真 VUID `presentable VkImage ... has not been acquired`）：
   读回发生在帧后，而 acquire/present 在渲染阶段内开合 ⇒ 需引擎侧「渲染后 present 前」回读窗口 + 非 acquire 态 fail-fast 护栏。详见 `doc/csm-readback-and-leak-followup.md`。
2. **`[LEAK]` 报告不可观察**：示例无自动退出开关（`CSM_AUTOWALK` 是速度），headless 只会被 timeout 杀掉
   ⇒ 退出期报告没机会打印。需先加帧上限开关，再用引擎已有的按类型汇总（`src/Vulkan/VKDevice.cpp:205-227`）分类。
3. **ViewportInfo 每帧多份化**未做（与 BDA 化任务有耦合）。
4. Scene 集剩余 5 个 UBO 待彻底 BDA 化（见交接文档；范式 = L2W 的 `pc_root.addr_l2w`）。

## 9. 改动禁忌（踩过的坑）

- **生成物禁止手改**：`ShaderLibrary/common/descriptor_macros.glsl` 由 `DescriptorMacroGen` 依真源
  （`DescriptorSetTypeDef.h` 枚举 + `kDescriptorBindingMacros` 表）生成，手改会让内容哈希契约红、
  产物缓存串味（表现为「语义与名字/结构错配」「资源凭空掉行」）。改真源 → `--emit` → `--verify`。
- **并行有序表是位置耦合**：`names[] ↔ semantic_values[]`、目录行 ↔ 枚举，删中间项会静默错位；必须成对删改。
- **别用行号定位补丁目标**（编辑途中文件会偏移）；本仓 CRLF，多行补丁易失配，优先单行锚点或带断言的字节级替换。
- 提交：`git add -u -- <dirs>` 后**不带 pathspec** 提交（`git commit -F msg -- <paths>` 会漏文件）；
  原生 git 读不到 `/tmp` 路径；`index.lock` 偶发竞争，清锁重试。
