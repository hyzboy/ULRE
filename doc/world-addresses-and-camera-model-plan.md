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
- **GLSL（C1-1/C1-2/C1-3 已落地）**：`WorldAddressesRef`（与 `GlobalAddressesRef` 并列，`buffer_reference_align=16`）已在 `scene_ubo.glsl`；宏为
  `camera = CameraInfoBufferRef(world_addresses.addr_camera_info).cameras[pc_root.camera_row]`、
  `global_render_items = RenderItemBufferRef(world_addresses.addr_global_render_items)`、
  `draw_item_ids = DrawItemIDBufferRef(world_addresses.addr_draw_item_ids)`，
  `viewport = ViewportInfoRef(global_addresses.addr_viewport)`（保持全局）。
  表内 `addr_sky` / `addr_shadow` / `addr_env` 槽位**已预留、尚未接线**（C2 落地）。
- **硬规矩**：`GlobalAddresses` 内**不得**出现世界私有地址；`WorldAddresses` 内**不得**出现资源池地址。各配一条契约（parity + 归属）。

## 2. 相机存储（世界私有）

- 世界私有 SSBO；行空间 = `kWorldCameraSlotCap(16) × HGL_FRAME_SLOT_TOTAL(8) = 128 行` × `sizeof(CameraInfo)`。
- 行号 = `camera_slot × 帧槽总数 + slot`（**世界内**）；shader 侧不变，仍由 `pc_root.camera_row` 索引。
- **0 号槽 = 本世界默认相机专属**；1..15 由世界内分配器（free list / 位图）分给普通相机、灯光相机、镜子相机、系统内建相机。
- **槽的拥有者负责归还（C3 落地）**：槽 = 「世界存储里的一行 + 一个拥有者」，两条路径覆盖全部相机：
  - **实体/作者持有的相机**（`CameraComponent` 在组件注册表里）：`CameraSystem::EnsureCameraSlot()` 认领槽，并把归还挂钩挂到组件上（`CameraComponent::slot_releaser`）——**组件析构即归还**（相机实体反复创建/销毁不会漏空 16 槽）；相机**升格为默认相机**时先交回旧的普通槽（`EnsureCameraSlot(is_default)`）。
  - **系统内建相机**（灯光 / 镜子：`EnvironmentSystem` 的 `light_camera`、示例自建光相机等，**不在**组件注册表 ⇒ 不会被 `CollectCameras()` 看到）：由拥有者持一个 **`CameraSlotGuard`**（RAII）——构造申请、`Reset()`/析构归还、只可移动不可拷贝；`EnableMainLightShadow` 申请、`DisableMainLightShadow` 归还。申请失败（槽耗尽 / 存储未就绪 / 世界为空）**报错留痕并保持 `kInvalidSlot`**：`BindTo()` 不碰相机 ⇒ 发布时跳过并一次性告警（fail-fast，绝不按越界行号写到别的世界）。申请/归还是对称的一条日志（`[CameraSlotGuard] 相机槽已申请/已归还 owner=… slot=N`）。
  - **安全网是弱引用，不是「世界销毁时通知」**：`CameraSlotGuard` 与 `slot_releaser` 都只持 `weak_ptr<CameraInfoStorage>`（世界相机行存储随之改为 `shared_ptr`，`ECSContext::GetCameraInfoStorageWeak()`）。世界先销毁（含**从未 Initialize ⇒ `Shutdown` 走早退分支**这一路）时 `lock()` 失败 ⇒ 析构自动 no-op。**禁用**任何 guard/挂钩注册表 —— 实测那条路会留下悬垂指针（`world->UnregisterSlotGuard` 访问已释放内存 ⇒ 段错误）。
- 容量 16、超限报错不扩容（既有约定）。

## 3. 相机解析（渲染必须要一个相机）—— **C1-4 已落地**

`CameraSystem::SelectMainCamera(cameras)`（每帧 / 每 pass 解析，结果即"本 pass 生效相机"）：

1. **0 号槽的默认相机**（`ECSContext::GetDefaultCamera()`，含常驻 fallback）仍在组件集合里 → 用它
   （跨帧稳定：相机集合顺序变化 / 新增相机都不会把主相机换掉）；
2. 否则**显式指定的主相机**（`camera->is_main_camera = true`，示例搭建期的写法）；
3. 否则**已加载实体中 `EntityID` 最小**的相机（同实体多相机按组件注册顺序）；
4. 仍无 → `ECSContext::EnsureFallbackCamera()`：在 `(0,0,0)` 强制生成 **常驻** fallback 相机
   （占 0 号槽、不进 `component_registry`、创建后一直活着），并在日志里说明。

选中的相机会**认领 0 号槽**（`ClaimDefaultCamera()` → `ECSContext::SetDefaultCamera()`；世界用
`weak_ptr` 持有 ⇒ 相机实体销毁后自动失效，不会让地址复用者"继承"默认相机身份）。常驻 fallback 被
实体相机顶替时会**交回 0 号槽**（槽是唯一的）。

- **槽语义（唯一真源 = `CameraComponent`）**：`kDefaultSlot = 0`、`kSlotCapacity = 16`、
  `kInvalidSlot = UINT32_MAX`，`HasCameraSlot()` 判定；`CameraInfoStorage` 的常量是它们的别名 +
  `static_assert` parity。**未分配 ≠ 0**：0 是合法槽号（本世界默认相机专属）。
- **pass 相机必须属本世界（fail-fast）**：`CameraComponent::world_owner`（认领槽的那个世界）在
  `RenderTo(req.camera)` 的覆盖分支里比对，跨世界直接报错拒绝（契约 §6.6②）。
- **发布路径也要能自愈**：`PublishCameraRows()` 由 `PrepareRenderPassSetup` 调用，**可能早于本帧的
  `CameraSystem::Update`**（第一帧就是），因此它先做一次三级解析 + `EnsureCameraSlot()` 认领槽（都幂等），
  否则相机的槽还是"未分配"⇒ 发布被跳过 ⇒ 主帧读到空行。
- **⚠ viewport 只在 tick / pass 覆盖上下文里绑**：`BindCameraResources()`（绑 viewport + 数据载体）与
  `EnsureCameraSlot()`（只认领槽）**必须分开**——发布路径也会在离屏 pass 的设置阶段被调用，那时
  `viewport_info` 是**离屏 RT** 的，绑上去会让主相机用错投影（实测：ATS 的 D1 bbox 112x58 → 146x77、
  D3 受影像素 18189 → 30766）。

CPU 侧消费者（剔除 / gizmo / Line 视锥 / shadow origin）统一通过"本 pass 生效相机"取数（`GetActiveCameraInfo()`），**不再**读世界共享的相机载体（该载体在 C1-5 删除）。

## 4. 环境（Env / Sky / Shadow 随世界；viewport 全局）

- `sky` / `shadow` 地址写进**世界表**（`addr_sky` / `addr_shadow`）；shadow 每槽一份 ring（沿用 `kShadowUboRing = 8`）。
- **profile 的所有权与生命周期跟随世界**：世界创建时选/建自己的 profile、销毁时归还；内置 default 可共享。
  现状是设备级配置仓库 + "世界/RT 只持有 `EnvProfileID`"（`EnvironmentManager.h`、`OffscreenWorld.h:59`、`RenderTargetDesc.h:82`），**地址发布却走全局字段** ⇒ 多世界同帧只有最后解析的那个 profile 生效。本轮落点就是把地址与选择按世界走。
- 收益：不同世界可用不同阴影技术（室内/室外）成为一等支持；消除跨世界互踩。

### 4.1 C2 已落地（2026-09-29）

| 面 | 落地 |
|---|---|
| 表 | `WorldAddresses` 增 `addr_sky` / `addr_shadow`（5 字段 40B）；`GlobalAddresses` 删这两项（8 字段 64B → **6 字段 48B**）；`scene_ubo.glsl` 的 `sky` / `shadow` 宏改读 `world_addresses`；归属分类表（门 `kAddressOwnershipTable`）两项移入世界侧；三份夹具 + `static_assert/offsetof` 同步 |
| 选择层 | `ECSContext::GetEnvProfileID()`：**显式覆盖优先**（`SetEnvProfileID` / `CreateEnvProfile`），否则**按需解析本世界 RT 的 `env_profile`**（作者侧 `SetEnvironmentProfile` 可能发生在世界创建之后 ⇒ 不能只取一次快照）；`EnvironmentSystem::ResolveProfileID()` 改问世界 |
| 发布 | `ECSContext::SyncWorldAddresses()` 按本世界 profile 取址写本世界槽：`EnvironmentManager::GetSkyAddress(profile)`（sky 单份 ⇒ 全帧槽同址）+ `GetShadowAddress(profile, 帧槽)`（ring 下标 = 帧槽）；取不到（0）时保留上一份好值（0 地址 = shader 解引用 0 基址 UB） |
| 生命周期 | `EnvironmentManager::Release(id)`（释放 sky + shadow ring 并从注册表移除；`default` / 无效句柄 no-op）；世界 `Shutdown` 归还**自己创建**的 profile；`SetEnvProfileID` 换 profile 时先归还旧的（不泄漏） |
| 删除（零兼容） | `GlobalSSBOBufferRegistry` 的 sky/shadow 槽 + `SetSkyAddress`/`SetShadowAddress`；`RenderSceneUBOSystem::ResolveSkyUBO`/`ResolveShadowUBO`（死代码）；`EnvironmentManager::GetSkyUBO`/`GetShadowUBO`；`Materialize*UBO` 里的地址注册（改为物化处只做取址 fail-fast） |
| `addr_env` | **暂不落**：当前不存在"env"（非 sky/shadow）的 GPU buffer 与着色器消费者，加一个恒 0 字段就是死槽；等真有 env UBO 再入表（归属表同一处登记）。 |

验证（实测）：双世界 `RenderToTextureColorDepth` —— 主世界 `profile=1 sky=0x304780000`、离屏世界 `profile=2 sky=0x309080000`（**地址不同 = 各自生效**，C2 之前全局表只能表达一个）；门 42/0、`TestCSMIncrementalPass` 22 Passed、RIDS rc=0、ATS 三契约与基线一致、CSM 对拍 8 轮 `不一致=0`、9 个示例（含 4 个 sky 示例）0 VUID。

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
6. 新增契约：①渲染期必解析出相机（无相机世界走 fallback 且相机在 `(0,0,0)`；**C1-4 已落地**：
   `TestCSMIncrementalPass` 的 9C+ 用例断言 fallback 常驻 / 占 0 号槽 / 不进组件表）；
   ②`req.camera` 必须在本世界相机存储中占槽（跨世界 = fail-fast；**C1-4 已落地**：
   `CameraComponent::world_owner` 在 `CameraSystem::Update` 的覆盖分支比对）；③两个世界同帧互不污染
   （相机行 / 4-ID 渲染项 / DrawItemID；**C1-6 已落地**：`TestCSMIncrementalPass` Test 22 共 19 条源码契约
   钉住"四个世界私有存储/地址表均为每世界一份、世界表只取本世界存储地址 + 按帧槽轮转、相机行只走本世界
   存储 + 校验 `world_owner`、禁复活设备级相机行池与全局表世界私有字段"；sky / shadow / env 的按世界归属随 C2）；
   ④表归属（**C1-6 已落地**：门的两条 `S./W.*-field-ownership` 用例，唯一真源 = `kAddressOwnershipTable`
   —— 按侧比对字段集合：多出 = 未分类字段、缺少 = 漏字段、出现在另一侧 = 放错表；已做破坏验证：
   把世界私有字段塞进全局表 / 给世界表加未分类字段都让门 rc=1 并报出对应诊断）。

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

## 8. 执行进度（每批落地后更新；验证数字为实跑）

### 8.1 已完成

| 批次 | 状态 | 落地内容 | 验证 |
|---|---|---|---|
| C1-1 | ✅ | `CameraInfoStorage`（世界私有 128 行相机行存储）+ `WorldAddresses.h` + `ECSContext` 持有/创建 + CMake 登记 | purge → ShadowMap 构建 rc=0 → 冒烟 0 VUID |
| C1-2 | ✅ | 世界表 SSBO（8 槽 × `kWorldAddressesSlotStride`）+ `pc_root.addr_world_addresses` + 三处 push（材质/线/文本批）+ `SetFrameIndex` 内 `SyncWorldAddresses` + 门 `W.world-addresses-struct-parity` | 门 **40 PASS / 0 FAIL**；`TestCSMIncrementalPass` 21；`TestRenderItemDataStorage` rc=0；ATS 三契约与基线一致；CSM 对拍 8 轮 `不一致=0`；双世界冒烟 0 VUID |
| C1-3 | ✅ | 相机行写入改走世界存储（`CameraSystem::PublishCamera` → `CameraInfoStorage::WriteCameraRow`、相机槽申请/归还 → `AcquireCameraSlot`/`ReleaseCameraSlot`）；GLSL `camera`/`global_render_items`/`draw_item_ids` 宏切到世界表；**删**全局表三字段（`addr_camera_info`/`addr_global_render_items`/`addr_draw_item_ids`）、`GlobalSSBOType::CameraInfo` 行池 + 相机号位图 + `AcquireCamera`/`ReleaseCamera`/`CameraRow`/`WriteCameraRow`/`WriteCamera`/`GetCameraInfoGPUBase`/`GetCameraInfoBuffer`/`UpdateRenderItemAddresses`/`RenderSceneUBOSystem::SyncGlobalAddressesTable`；`GlobalAddresses` 88B → **64B** | 同上全套 + `TestRenderItemDataStorage` 的 Test 8 改为断言两张表的新布局（88B→64B / 世界表 24B） |
| C1-4 | ✅ | 相机模型：三级解析（默认相机 → 显式 `is_main_camera` → 最小 `EntityID` → 常驻 fallback）+ 0 号槽专属默认相机 + `kInvalidSlot` 哨兵（删"0 = 主相机 / 未分配"双关）+ 槽唯一真源上移到 `CameraComponent` + `world_owner` 跨世界 fail-fast + `EnsureCameraSlot()`（发布路径只认领槽，不绑 viewport） | 门 **40 PASS / 0 FAIL**；`TestCSMIncrementalPass` **21 Passed**（含新 9C+ 相机模型契约）；`TestRenderItemDataStorage` rc=0；ATS 三契约与基线逐项一致（D1 112x58 / 57.6%、D3 18189 & 600662、D4 0、0 VUID）；CSM 对拍 8 轮 `不一致=0`；双世界 + ShadowMap 冒烟 0 VUID、0 槽耗尽/未认领/跨世界告警 |
| **C1-4a** | ✅ | **相机视图矩阵修复**（`CameraSystem::UpdateMatrices` 非 custom 分支）：视图一律用 `LookAtMatrix(position, position + forward, world_up)`，**禁用**可能"慢一拍"的 `target`（`position` 被外部直接写时 `target` 落后一帧 ⇒ 方向差 ~30° ⇒ 该帧整幅渲成另一机位；实测同姿态 `viewT` 1.2m ↔ 13.5m 横跳 = "隔几秒拉扯一次/刚出场抖"）。成因与排查法见 `doc/csm-mechanism.md` §5 | `viewT` 序列变为单调平滑；200 帧逐帧转储位移互相关 **0 跳变**；门 40/0；21 Passed；RIDS rc=0；ATS D1 112x58/57.6% 一致（D3 18187 vs 18189，−2px float 末位）；CSM 对拍 8 轮 `不一致=0`；三示例 0 VUID |
| C1-5 | ✅ | **删世界共享相机载体**：`CameraSystem::camera_info` / `camera_ubo`(+`camera_ubo_managed` / `EnsureCameraResources` / `Shutdown` 释放块) / `GetCamera` / `GetCameraInfo` / `GetCameraUBO` / `CommitCameraUBO` / `UpdateMatrices` 里"主相机/覆盖相机写共享载体"的兼容分支整删；`WorkObject::GetCamera/GetCameraInfo` 删；**30 个示例文件 64 行别名赋值**（`camera->camera_data = GetCamera(); camera->camera_info = const_cast<...>(GetCameraInfo());`）全删——`CameraComponent` 构造即自指向 `local_camera_data/local_camera_info`，`BindCameraResources` 兜底同一件事；新增 **`ECSContext::GetActiveCameraInfo()`**（pass 覆盖相机优先，否则本世界主相机）供剔除 / gizmo / Line 视锥 / shadow origin 统一取"本 pass 生效相机"；`RenderPrimitiveCollectSystem::cameraInfo` 由"安装期缓存指针"改为**每帧现取**（`SetCameraInfo` 接线全删，`DefaultSystems` / `OffscreenWorld` 不再灌指针）；`ViewUBOCommitSystem` 不再提交相机 | 门 **40 PASS / 0 FAIL**；`TestCSMIncrementalPass` **21 Passed**；RIDS rc=0；ATS 三契约与基线逐项一致（D1 112x58/57.6%、D3 18187 & 19109、D4 0、0 VUID）；CSM 对拍 8 轮 `不一致=0`；7 个改过的示例（ShadowMap / RenderToTexture / RenderToTextureColorDepth / SimpleCube / ComputeFrustumCull / RayPicking / GizmoUsageExample）冒烟 0 VUID 且启动日志量与改前一致；全仓 `grep GetCameraInfo()` 只剩 `GetActiveCameraInfo()` |
| **C1-6** | ✅ | **契约收口**：门加两条**表归属**用例（`S.global-addresses-field-ownership` / `W.world-addresses-field-ownership`，唯一真源 = `kAddressOwnershipTable`：按字段集合比对，多出来的是"未分类字段"、少了的是"漏字段"、出现在另一侧的是"放错表"）；`TestCSMIncrementalPass` 加 **Test 22** 两世界同帧隔离契约（19 条源码契约：四个世界私有存储/地址表均每世界一份、世界表只取本世界存储地址且按帧槽轮转、相机行只走本世界存储 + 校验 `world_owner`、设备级相机行池与全局表世界私有字段禁复活）| 门 **42 PASS / 0 FAIL**（+2）；`TestCSMIncrementalPass` **22 Passed**；**破坏验证**已做：把 `addr_camera_info` 塞进全局表 ⇒ rc=1 且报"放错表"、世界表加未分类字段 ⇒ rc=1 且报"未分类字段"，还原后逐字节一致、门回落 42/0；RIDS rc=0、ATS 三契约与基线一致、CSM 对拍 8 轮 `不一致=0`、7 示例冒烟 0 VUID |
| **C2** | ✅ | **Env 归世界**（见 §4.1）：`addr_sky` / `addr_shadow` 进世界表（`GlobalAddresses` 64B→**48B**）、`sky`/`shadow` 宏改读 `world_addresses`；选择层收敛到世界（`ECSContext::GetEnvProfileID()`）；发布走 `SyncWorldAddresses` + `GetSkyAddress/GetShadowAddress`；`Release(profile)` + 世界归还自有 profile；删 registry 两个 setter / `ResolveSkyUBO/ResolveShadowUBO` / `GetSkyUBO/GetShadowUBO` | 门 42/0；22 Passed；RIDS rc=0；ATS 与基线一致；CSM 对拍 8 轮 `不一致=0`；**双世界地址实测分离**（`0x304780000` vs `0x309080000`）；9 示例 0 VUID |
| **C3** | ✅ | **灯光/镜子相机通用化 = 槽的拥有者生命周期**（见 §2）：新增 `CameraSlotGuard`（RAII：申请/归还/失败留痕/只可移动/弱引用安全网，`inc/hgl/ecs/support/CameraSlotGuard.h` + `src/ecs/support/CameraSlotGuard.cpp`，CMake 已登记）；`CameraComponent` 加 `slot_releaser`（析构即归还非 0 槽）+ 析构实现；`CameraSystem::EnsureCameraSlot` 挂挂钩 + **升格默认相机先交回旧槽**；`EnvironmentSystem` 光相机改由 `light_camera_slot` 持有（Enable 申请 / Disable 归还，手工 `Acquire/Release` 对删除）；世界相机行存储改 `shared_ptr` + `GetCameraInfoStorageWeak()`（弱引用安全网，**禁用**注册表/Detach 方案）；`TestCSMIncrementalPass` 加 **Test 23**（分配/复用/耗尽拒绝/组件析构归还挂钩/guard 未就绪不下发/世界先销毁 no-op + 18 条源码契约），Test 10D 契约改为断言 guard 路线 | 门 **42 PASS / 0 FAIL**；`TestCSMIncrementalPass` **23 Passed / 0 Failed**；RIDS rc=0；ATS 三契约与基线**逐项一致**（D1 112x58/57.6%、D3 18187 & 19109、bias 600662、D4 0、**0 VUID**）；运行时实测 `[CameraSlotGuard] 相机槽已申请 owner="AutoCSMLightCamera" slot=1`，探针实测世界存活时 `DisableMainLightShadow()` ⇒ `相机槽已归还 slot=1`（同槽、契约不变）；5 示例（ShadowMap/CascadeShadowMap/RenderToTexture/RenderToTextureColorDepth/SimpleCube）0 VUID/0 设备丢失 |

### 8.2 待办（按依赖排序）

| 批次 | 目标 | 主要落点 | 判据 |
|---|---|---|---|
| **C4** | ComponentData 骨架：先 `CameraComponent` → `CameraData` + 世界级存储 + 槽访问器，再 Transform / Geometry / Material | 与 C1-5 合并代价小（都是相机存储收口），但回归面变宽 ⇒ 建议 C1-5 先落地 | 门 + 全示例 |

### 8.3 基线与验证命令（改 CSM / 相机后逐项跑）

```
# 基线：门 42 PASS / 0 FAIL（含 S/W 两条 *-field-ownership）；TestCSMIncrementalPass 23 Passed（Test 22 = 两世界隔离、Test 23 = 相机槽拥有者生命周期）；ATS D1 112x58 57.6% / D3 18187~18189 & 600662 / D4 0 / 0 VUID
cmake --build build --config Debug --target ShadowMap AlphaTestShadow CascadeShadowMap TestCSMIncrementalPass TestRenderItemDataStorage ShaderResourceSchemaRegressionGate
./build/out/Windows_64_Debug/ShaderResourceSchemaRegressionGate.exe
./build/out/Windows_64_Debug/TestCSMIncrementalPass.exe          # 必须 cwd=仓库根（Test 7C 读 ShaderLibrary/）
ATS_SELFCHECK=1 ./build/out/Windows_64_Debug/AlphaTestShadow.exe
CSM_CACHE_DIFF=1 CSM_AUTOWALK=4 ./build/out/Windows_64_Debug/CascadeShadowMap.exe   # 判据：多轮 不一致=0（需 timeout）
```
相机/视口类改动的追加核对：解算处打印 `camera_info->view[3]` + `pos/target/forward`（**同一姿态下 `viewT` 必须一致**，且随位置单调）；必要时逐帧转储颜色 + 亮度剖面位移互相关（判据：0 跳变）。这些打印/转储都是**临时**手段，定位完即删。
槽相关改动（C3 之后）：跑 `CascadeShadowMap`/`AlphaTestShadow` 看 `[CameraSlotGuard] 相机槽已申请 … slot=N`（每个内建相机一条）；世界存活时 `DisableMainLightShadow()` 应打 `相机槽已归还 … slot=N`（同槽）；槽耗尽/越界/重复归还在 `TestCSMIncrementalPass` Test 23 里是行为契约。
**已知的预存在问题**（C3 探针暴露，与相机槽无关，未被任何示例走过）：① 同一会话反复 `EnableMainLightShadow` ⇒ `[RenderTargetManager] CSM_Cascade_0: in-flight 槽带越界（起点=8 槽数=1 上限=8）`（RT 的 in-flight 槽带不回收）；② 运行中 `DisableMainLightShadow` ⇒ 2 条 `vkDestroySemaphore(): VkSemaphore[CSM_Cascade_0:Lane] that is currently in use by VkQueue`（级联 RT 的 timeline 车道在队列仍在用时销毁）。
改头文件/结构大小后先 `purge-stale-deps.sh`；清 `build/cache-hot/shader-cache`；**禁用 `| grep error` 判构建结果**。
**改了类布局的头之后，purge 不够时清整棵 obj 树**：本轮 C3 给 `EnvironmentSystem` 加成员后 purge 过仍崩在 `EnvironmentSystem::ResolveManager → GraphicsContext::GetEnvironmentManager`（读坏 `render_context`）；`find build -type d -path "*.dir/Debug" -print0 | xargs -0 rm -rf` 后同二进制恢复正常 ⇒ 判定「陈旧 TU」而非逻辑 bug。另：**框架初始化失败的退出码是 127**（不是 bash 的 command not found），日志尾部只有一行真实原因，别把它当脚本错误。

### 8.4 提交范围（本轮已本地提交，未推送）

`C1-1/C1-2`（23:43–23:52）→ `C1-3`（00:38–00:42）→ `C1-4`（含 9C+ 契约与文档）→ **C1-4a 相机视图修复** `8744f004a`（02:31）→ **删诊断设施** `8c55e5b63` → **C1-5 删世界共享相机载体** `6a2a376a2` → **C1-6 契约收口** `629b63e22` → **C2 Env 归世界** `8b628d8e1`。**C3 已落地但尚未提交**（9 文件改 + 2 新文件：`CameraSlotGuard.h/.cpp`）。
