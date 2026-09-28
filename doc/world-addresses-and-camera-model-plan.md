# Global / World 双地址表 + 相机模型（决策与执行计划）

> 权威口径文档。决策人：hyzboy（2026-09-28 会话）。落地前请先读本文，再读
> `doc/global-addresses-bda-unification-plan.md`（S1–S3：Scene 集退役 + 地址归口 GlobalAddresses）。
> 本文同时收录**本轮核出的文档事实订正清单**（§7）——其它文档按该表对齐，不要再沿用旧写法。

## 0. 决策记录（用户拍板，勿翻案）

1. 渲染**必须**有一个相机：三级解析（默认相机 → 最小实体号相机 → `(0,0,0)` 强制 fallback），见 §3。
2. 光相机**随"投影阴影的灯光"创建**；未来"镜子"同理 ⇒ **相机 = 世界相机存储里的一个槽 + 一个拥有者**。
3. **Global / World 两张地址表**（不合并成一张）。
4. **sky 随世界、Shadow 随世界、Env 随世界；viewport 全局**。
5. **0 号相机槽 = 默认相机专属**（把"0 = 主相机"的编号语义保留下来，但语义从"全局唯一主相机"改为"本世界默认相机"）。
6. 世界相机容量 **16 槽**（现状 8 = 64 行 ÷ 8 槽已不够：每个投影阴影的灯光、镜子都要占槽）；超限 **fail-fast**，不预留增长。
7. fallback 相机**常驻**（惰性创建后一直占 0 号槽，不按需创建/销毁）。
8. "最小号的相机" = **`EntityID.index` 最小**的实体所拥有的相机（同实体多个相机按组件注册顺序）。

## 1. 三层地址分层（定稿）

| 档 | 载体 | 作用域 | 槽形状 | 内容 |
|---|---|---|---|---|
| 跨世界共享 | `GlobalAddresses`（1 张 / GraphicsContext） | 设备 | 8 槽 × 128B（现有） | 资源池与全局单份：`mesh_draw_params_pool`、`pbr_surface`、`emissive_surface`、`transmission_surface`、`color_palette`、**`viewport`** |
| 世界私有 | `WorldAddresses`（1 张 / 世界，世界私有 SSBO） | 世界 | 8 槽 × 128B（**与全局表同形，复用同一套槽算术与常量**） | `camera_info`、`global_render_items`、`draw_item_ids`、`sky`、`shadow`（每槽一份 ring）、`env` |
| 每批 / 每材质 / 本字体 | `pc_root`（push constants） | 批 | — | `addr_global_addresses`、**`addr_world_addresses`**、`addr_batch_mesh_draw_params`、`addr_mtl_data_addrs`、`addr_texture_references`、`addr_text_char_info/style/instance`、`camera_row` |

**判据一句话**：跨世界共享的**资源** → `GlobalAddresses`；每个世界独有的**观察者 / 状态** → `WorldAddresses`；同帧内**逐批变化**的 → `pc_root`。

- **下发**：`pc_root.addr_world_addresses` = 本帧槽的世界表地址（`world->GetWorldAddressesAddress(frame_slot)`，与 `GraphicsContext::GetGlobalAddressesAddress(frame_slot)` 同款）；多世界渲染只需换这一个指针。
- **GLSL**：新增 `WorldAddressesRef`（与 `GlobalAddressesRef` 并列，`buffer_reference_align=16`）；宏改为
  `camera = CameraInfoBufferRef(world.addr_camera_info).cameras[pc_root.camera_row]`、
  `sky = SkyInfoRef(world.addr_sky)`、`shadow = ShadowInfoRef(world.addr_shadow)`，
  `viewport = ViewportInfoRef(global_addresses.addr_viewport)`（保持全局）。
- **硬规矩**：`GlobalAddresses` 内**不得**出现世界私有地址；`WorldAddresses` 内**不得**出现资源池地址。各配一条契约（parity + 归属）。

## 2. 相机存储（世界私有）

- 世界私有 SSBO；行空间 = `kWorldCameraSlotCap(16) × HGL_FRAME_SLOT_TOTAL(8) = 128 行` × `sizeof(CameraInfo)`。
- 行号 = `camera_slot × 帧槽总数 + slot`（**世界内**）；shader 侧不变，仍由 `pc_root.camera_row` 索引。
- **0 号槽 = 本世界默认相机专属**；1..15 由世界内分配器（free list / 位图）分给普通相机、灯光相机、镜子相机、系统内建相机。
- **拥有者负责归还**：灯光销毁 / 镜面销毁 / `DisableMainLightShadow` 等对称归还（现状 CSM 的 `ReleaseCamera` 对称释放即此规则的单灯特例）。
- 容量 16、超限报错不扩容（既有约定）。

## 3. 相机解析（渲染必须要一个相机）

`ResolveDefaultCamera()`（每帧 / 每 pass 解析，结果落到"本 pass 生效相机"）：

1. 0 号槽的**默认相机**存在 → 用它；
2. 否则取**已加载 Entity 中 `EntityID.index` 最小**的相机 → 用它；
3. 仍无 → 在 `(0,0,0)` **强制生成 fallback 相机**（占 0 号槽、常驻）。

CPU 侧消费者（剔除 / gizmo / Line 视锥 / shadow origin）统一通过"本 pass 生效相机"取数（`GetActiveCameraInfo()`），**不再**读世界共享的相机载体（该载体本轮删除）。

## 4. 环境（Env / Sky / Shadow 随世界；viewport 全局）

- `sky` / `shadow` / `env` 三项地址写进**世界表**；shadow 每槽一份 ring（沿用 `kShadowUboRing = 8`）。
- **profile 的所有权与生命周期跟随世界**：世界创建时选/建自己的 profile、销毁时归还；内置 default 可共享。
  现状是设备级配置仓库 + "世界/RT 只持有 `EnvProfileID`"（`EnvironmentManager.h:20`、`OffscreenWorld.h:59`、`RenderTargetDesc.h:82`），**地址发布却走全局字段** ⇒ 多世界同帧只有最后解析的那个 profile 生效。本轮落点就是把地址与选择按世界走。
- 收益：不同世界可用不同阴影技术（室内/室外）成为一等支持；消除跨世界互踩。

## 5. 执行批次（每批自身可编译 + 门绿）

| 批次 | 内容 |
|---|---|
| **C0** | 提交 ShadowMap 相机行污染修复：`CameraSystem::RestoreMainCamera()` 改为**重新解算**主相机（原来是自拷贝，导致主帧相机行带离屏相机数据）。单文件、已验证。 |
| **C1** | **表 + 相机收口**：`WorldAddresses` 落地（camera / render items / draw item ids / sky / shadow / env 地址归口）+ 相机存储下沉世界级 + 三级解析 + 0 号槽默认相机 + 容量 16 + 删世界共享相机载体 / `WorkObject::GetCamera/GetCameraInfo` / 28 示例别名 + `GetActiveCameraInfo()` + `req.camera` 必须属本世界。 |
| **C2** | **Env 归世界**：profile 生命周期随世界 + `ResolveSkyUBO/ResolveShadowUBO` 改按世界 + `RenderSceneUBOSystem` / `ViewUBOCommitSystem` / `EnvironmentSystem` 取数点改造。 |
| **C3** | **灯光 / 镜子相机通用化**：每个投影阴影的灯光 → 申请一个相机槽（申请/归还随灯光生命周期）；对齐 `doc/shadow-component-and-automated-pipeline-design.md` 的三层设计。 |
| **C4** | **ComponentData 骨架**：先做 CameraComponent 样板（`CameraData` + 世界级存储 + 槽访问器），再依次 Transform / Geometry / Material。 |

C1 与 C4 可合并为一批（都是相机存储重构），代价是回归面变宽；建议分开。

## 6. 验收门

1. 门：`ShaderResourceSchemaRegressionGate`（表结构变更需同步 golden/schema；新增 `W.world-addresses-struct-parity` 与"表归属"契约）。
2. `TestCSMIncrementalPass`、`TestRenderItemDataStorage` 通过。
3. `ATS_SELFCHECK=1 AlphaTestShadow`：D1/D3/D4 契约 + selfcheck PASS + 0 VUID。
4. `CSM_CACHE_DIFF=1 CSM_AUTOWALK=4 CascadeShadowMap`：多轮 `不一致=0`。
5. 示例冒烟：`ShadowMap`（单世界）+ `RenderToTexture` / `RenderToTextureColorDepth`（双世界）0 VUID。
6. 新增契约：①渲染期必解析出相机（无相机世界走 fallback 且相机在 `(0,0,0)`）；②`req.camera` 必须在本世界相机存储中占槽（跨世界 = fail-fast）；③两个世界同帧互不污染（相机行 / sky / shadow / render items 地址）；④表归属。

## 7. 事实订正清单（本轮核对；其它文档按此对齐）

| 过期写法 | 实际（含出处） |
|---|---|
| `RootAddresses` 72B / 56B；首字段 `addr_mesh_draw_params` | **80B**；首字段 `addr_global_addresses`，第二位 `addr_batch_mesh_draw_params`，末位 `camera_row` + `_pad_camera`（`inc/hgl/graph/ShaderBufferSources.h:266-276`） |
| `GlobalAddresses` 是 Set 0 / binding 4 的 56B（或 64B）**UBO** | **SSBO + `HGL_FRAME_SLOT_TOTAL` 帧槽 × 128B 步长、无绑定无集**（`inc/hgl/graph/ubo/GlobalAddresses.h:14-57`） |
| Scene 集（`SCENE_SET`）与 `VKGlobalSceneUBOSet` 仍在 | **已整体退场**；描述符集收敛为唯一 Bindless(0)（S3；`inc/hgl/common/DescriptorSetTypeDef.h:9-13`） |
| `scene_ubo.glsl` 的 `camera` 宏在 `:123`、字段名 `camera_id` | 行号已漂到 `:149`，字段已改名 **`camera_row`** |
| 相机行"按组件发布"因为"光源相机属另一个世界" | **错误归因**：光相机是 `EnvironmentSystem` 用 `make_shared` 创建、**不经 Entity/AddComponent 注册**，`CollectCameras()` 看不到它；CSM 始终是**一个世界 + 多个渲染过滤程**（`EnvironmentSystem.cpp:216/389/421`）。正确表述见 §3/§4 与 `CameraSystem.h` 注释。 |
| `MaterialInstanceAddresses` 8B 双 index | **16B 四字段** `{payload_index, texture_reference_index, shadow_bias_multiplier, shadow_flags}`（`ShaderBufferSources.h:142-152`） |
| 4-ID GPU 解析已启用 | **未启用**：`uses_render_item_resolve` 全仓无处置 true（`MaterialBatch.h:71` 只被置 false） |
| 相机行池 = 全局 64 行 + 全局相机号位图 + 全局 8 相机上限 | 本轮改为：**世界私有 16 槽 × 8 帧槽**（§2）；0 号槽恒为世界默认相机 |
| `EnvironmentManager` 的 sky/shadow 地址是"全局字段" | 本轮后 sky / shadow / env 地址按**世界**发布（§4） |
