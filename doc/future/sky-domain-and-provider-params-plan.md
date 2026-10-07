# 天空 / 环境光域（provider 参数块）正式形态方案

> **版次**：v1（2026-10-07）。范围 = 天空/环境光**数据面**的正式形态：provider 槽参数块的归位与规则、
> 参数块纳入世界表地址体系、逐帧演化时的帧槽口径、结构镜像对拍的通用机制。
> **不含**任何天空/天光**方法本身**（真大气散射、程序化天空、HDRI 天空、浑浊度模型等实现 = 另立项），
> 也不动已在 T6a/T6b 落地完成的光源行与影子域（`LightRow` / `ShadowInfo` / CSM）。
>
> **本稿的定位 = 交接档**：写它的目的是让**一个没有上下文的新会话**能独立把它执行完 ——
> 因此含判据基线（数值）、命令模板、已知坑、逐任务的接线面与回滚点。执行者请先读 §8 附录「执行须知」。
>
> **关联文档**：
> - `doc/future/lighting-shadow-domain-split-plan.md`（光影三域拆分；本稿是它的**后续**：光源/影子已定型，剩天空侧）
> - `doc/ENVIRONMENT_SYSTEM.md`（环境域现状：profile + sky UBO；本稿改变其「sky 段形态」一节）
> - `doc/world-addresses-and-camera-model-plan.md`（地址三档；本稿给世界表**再加一格** `addr_ambient_env`）
> - `doc/csm-mechanism.md`（CSM 机制，本稿不碰）
>
> **⚠ 落地记录（2026-10-07）：`sky` 段整删 —— 本稿 §0.3 / §2.2 / §3.2 / §4-T1 里「`SkyInfo` 保留外观标量」的形态已作废。**
> 用户拍板：`SkyInfo` 的两个字段（`base_sky_color` / `halo_intensity`）只服务「假大气天空」这一种方法，且
> **全仓没有一个写者**（唯一「调用点」是 `example/Environment/AtmosphereSkyMinimal.cpp` 的一句空 `EditSkyInfo()`，
> 连返回值都没接、未改任何字段）⇒ 取值搬成 `ShaderLibrary/sky/sky_atmosphere.glsl` 内常量
> `kBaseSkyColor` / `kHaloIntensity`，**外观逐位不变**；`SkyInfo.h` 删除、`addr_sky` 退出世界表
> （`WorldAddresses` 80 → **72 B**）、GLSL 镜像与 `#define sky` 删除、`EnvironmentManager` / `EnvironmentSystem`
> 的 sky 面删除、`EnvironmentInfo` 只剩 `ambient_env` 一段。
> **R1–R4 规则不变**（它们定义的正是「将来怎么做」），且本稿 **T2 的触发面因此缩到 `AmbientEnvInfo` 一段**（16 B）。
> 实跑判据见 §5.1；口径订正见 §7。

---

## 0. 设计输入

### 0.1 已定输入 —— 用户决策（2026-10-07）

| # | 决策 | 落点 |
|---|---|---|
| S1 | **（该结构已于 2026-10-07 整删；本决策作为「将来天空方法共用一个块」的规则保留）`SkyInfo` 的定位是"未来多种天空/天光方法共用的数据结构"**，不是"光照参数结构"；当前只有一个方向光，它存在的唯一理由是"避免场景太黑"（光照系统尚未正式开始做） | §1 / §2 |
| S2 | **现在不改结构，只把规则定下来**（不为未实现的功能造脚手架；等第三种 provider 或某个天空方法需要逐帧演化时再动） | §2 / §4 |
| S3 | **句柄类字段必须与外观标量分离成独立块**；`_pad` + `offsetof` 断言只是**兜底**，不是解法 | §2.1 R2 |
| S4 | 沿用零兼容纪律：**不保留双写期/兼容分支**；删除时全仓 grep 确认零残留 | §4 / §6 |

### 0.2 已定输入 —— 引擎不变量（违反即编译失败或渲染崩）

| # | 不变量 | 出处 |
|---|---|---|
| I1 | 地址三档：`GlobalAddresses`（跨世界池）/ `WorldAddresses`（世界私有）/ `pc_root`（每批）；shader 经 BDA 取数，**描述符集只剩 `pc_root` + Bindless=0** | `inc/hgl/graph/ubo/WorldAddresses.h`、`inc/hgl/common/DescriptorSetTypeDef.h` |
| I2 | 世界表当前 **9 字段 × 8 B = 72 B**（`static_assert(sizeof(WorldAddresses) == 72)` 在 `src/ecs/support/TestRenderItemDataStorage.cpp`），槽步长 `kWorldAddressesSlotStride = 128`、槽数 `kWorldAddressesSlotCount = HGL_FRAME_SLOT_TOTAL = 8`。**加一格字段 ⇒ 80 B，必须同批更新 sizeof 断言 + 逐字段 offsetof + 所有内嵌夹具** | 同上 |
| I3 | 容量冻结 + 超限 fail-fast（不扩容、不静默截断）；新增枚举必须 constexpr 全表覆盖判定 | 用户既定纪律 |
| I4 | 新地址字段**必须接写者**（`addr_l2w` 曾只有字段没写者 ⇒ 静默剔空），且发布口径与 `addr_lights` 同形 | `ulre-entity-gpu-cull` 约定 10 |
| I5 | **C++↔GLSL 共享结构不许依赖隐式对齐**：GLSL 是 `scalar` 布局（`uvec4` 只要求 4 对齐），GLM 的 `vec4/uvec4` 是 16 对齐 ⇒ 「`vec4` + `float` + `uvec4`」这种形状两端会差 12 B。症状**不是崩**，是越过句柄全 0 早退、拿 0 句柄采样 ⇒ 模块静默发黑 | 实测（见 §0.3「IBL 事故」） |
| I6 | 帧槽空间：主帧 `[0, HGL_FRAME_SLOT_MAIN=4)` / 离屏 `[4, HGL_FRAME_SLOT_TOTAL=8)`；per-frame ring 深度必须 **== 槽空间** | `inc/hgl/common/RenderOptions.h` |
| I7 | 改结构体 ⇒ **清 obj 树 + 清 shader-cache 全量重编**（MSBuild 依赖失效 ⇒ TU ABI 错位：LNK2019 / 首次调用即崩 / 测试读垃圾值） | `crlf-safe-editing` |
| I8 | 判据基线（本轮实测）：门 **46 PASS / 0 FAIL**；`TestCSMIncrementalPass` **46 个测试** rc=0；`TestRenderItemDataStorage` rc=0；八台架 + RTT/RTTCD 摘要逐位（数值见 §5） | 本稿实测 |

### 0.3 现状诊断（事实依据，逐条给 path:symbol）

- **（当时现状；该结构 2026-10-07 整删）`SkyInfo` 当时只有 4 个字段、48 B**（`inc/hgl/graph/ubo/SkyInfo.h`）：
  `Color4f base_sky_color`(0) + `float halo_intensity`(16) + `uint32_t _pad[3]`(20..32，**为对齐而存在的填充**) + `Vector4u env_tex`(32)；
  三条 GPU ABI 断言：`sizeof == 48` / `halo_intensity@16` / `env_tex@32`。

- **字段 → 消费者是一一映射，而且两个消费者属于两个不同的 provider 槽**：

  | 字段 | 唯一读者 | 那个读者的身份（模块元数据） |
  |---|---|---|
  | `sky.base_sky_color` / `sky.halo_intensity` | `ShaderLibrary/sky/sky_atmosphere.glsl`（各 1 处） | `@ulre slot **sky_provider**`，`provides_capability ambient_diffuse` |
  | `sky.env_tex`（3 个 bindless 句柄 + 1 预留） | `ShaderLibrary/lighting/indirect_ibl.glsl`（4 处） | `@ulre slot **ambient_light_provider**`，`provides_capability ambient_diffuse\|ambient_specular` |

  ⇒ **`env_tex` 不是天空外观数据**：它的消费者是「环境光 provider」，注释里也自称「场景全局」。
  它住在 `SkyInfo` 里是历史遗留（Scene 集时代），也正是 `_pad[3]` 存在的原因。

- **C++ 侧消费者只有两个示例**（全仓 grep `SkyInfo`）：
  `example/Environment/AtmosphereSkyMinimal.cpp`（`EnvironmentSystem::EditSkyInfo`）、
  `example/Environment/IBLEnvironment.cpp`（`EditSkyInfo` + 写 `env_tex`）。
  同族的 `AtmosphereSkyAmbient` / `AtmosphereSkySunGizmo` **完全不碰** `SkyInfo`。

- **C++ 通路**：`EnvironmentSystem::EditSkyInfo/GetSkyInfo/SetSkyInfo/MarkSkyDirty`（转发）→
  `EnvironmentManager`（`Profile::cpu.sky` + `sky_ubo` 懒物化 + `GetSkyAddress`）→
  `ECSContext::SyncWorldAddresses` 写 `WorldAddresses.addr_sky` → GLSL `#define sky SkyInfoRef(world_addresses.addr_sky)`
  （`ShaderLibrary/ubo/scene_ubo.glsl`）。

- **`addr_sky` 是"单份 buffer ⇒ 全帧槽同址"**（`WorldAddresses.h` 注释原文），只在"谁都不逐帧改它"时才安全。
  对照：光源行**按帧槽切份**（正是因为它每帧被改），影子数据也按帧槽一份。

- **判据覆盖面（重要，别误判）**：`base_sky_color` / `halo_intensity` **已被八台架摘要覆盖** —— 默认材质模板给
  片元着色器挂了 `sky_provider` = `sky_atmosphere`（注册于 `src/ShaderGen/template/SceneRenderTemplateResolver.cpp`），
  它提供的 `ambient_diffuse` 参与环境光，摘要因此对这两个字段敏感（实测证据：T6b 减法里删掉 `halo_color` 时
  **4 个台架摘要当场变化**）。
  **真正的盲区是 `env_tex`（IBL 句柄）**：它只被 `ShaderLibrary/lighting/indirect_ibl.glsl` 读，而 `indirect_ibl`
  只在 `IBLEnvironment` 里被选为 `ambient_light_provider` ⇒ 八台架与其余示例**都不经过这条路径**。
  同时，环境示例族 5 个示例**目前只有"0 VUID / 0 [ERROR]"这类非数值判据，没有帧摘要**。
  ⇒ 上一次的 IBL 事故（下条）正是从这个盲区里漏过去的：**没有任何摘要能发现它，只有目视。**

- **IBL 事故复盘（2026-10-07，T6b 减法后用户实测发现）**：`SkyInfo` 收紧为
  `base_sky_color + halo_intensity + env_tex` 之后，`env_tex` 在两端落到不同偏移 ——
  C++（GLM 16 对齐）在 **32**、GLSL（`scalar` 布局、`uvec4` 只要求 4 对齐）在 **20**
  ⇒ shader 读到 `env_tex.x = 0 / y ≠ 0`、越过"句柄全 0 早退"、拿**句柄 0** 采样 ⇒ IBL 全黑（物体只剩高光点）。
  旧布局没踩是因为 `uvec4` 前面恰有 4 个 float、落在 16 的倍数上。修法 = 两端显式 `_pad[3]` + 三条 ABI 断言
  （提交 `949234482`）。**教训即 I5/R2**；**能瞒过一次完整判据**的原因见上条（`env_tex` 是摘要盲区）。

### 0.4 待拍板项（有推荐值，不定也能开工）

| # | 议题 | 选项 | 推荐 |
|---|---|---|---|
| P1 | 搬迁是否分两批（加法/减法） | (a) **单批** (b) 分两批（加法=新块就位但 shader 未切 ⇒ 需双写以保持外观不变） | **(a) 单批**：只有 2 个消费者、改动面小；双写会在本次改造里**重新制造双真源**（与 S3/S4 相悖）。回滚靠 tag + `git revert` |
| P2 | 新块承载形式 | (a) 独立小块 + `addr_ambient_env`（推荐） (b) 做成"环境光 provider 行表"（照 `LightRowTable`，为未来多个 ambient provider 预留） (c) 塞进光源行的扩展区 | **(a)**：当前只有 1 个 ambient provider；行表是"多者并存"才需要的结构（I3：不为容量预留）。(b) 留到第二个 ambient provider 出现 |
| P3 | 新块命名 | `AmbientEnvInfo` / `IblEnvInfo` / `LightingEnvInfo` | **`AmbientEnvInfo`**：槽名是 `ambient_light_provider`，未来非 IBL 的 ambient provider 也用它（`IblEnvInfo` 会把块名和当前唯一实现绑死） |
| P4 | 是否先做 §4 T0（给两个消费者示例接帧摘要） | (a) 先做（推荐） (b) 直接改结构、靠目视 | **(a)**：把"目视 + 0 VUID"升级为可自动判定的数值判据，是本类结构搬家唯一的护栏（见 §0.3 判据盲区）。成本小（照抄既有 `CSM_SELFCHECK` 模式） |
| P5 | profile 与 provider 选择的关系 | (a) 现在就定 (b) 等第二/第三个 provider 出现再定 | **(b)**，但**先把方向写进档**：profile 只持有"每个 provider 槽选哪个模块/参数集"的**引用**，参数块本身**世界私有**（与 D2 同源：profile 可被多世界共享 ⇒ 参数块挂 profile 就会互踩） |

---

## 1. 目标与非目标

**目标**
1. 定下 `SkyInfo`（= `sky_provider` 槽参数块）与 `AmbientEnvInfo`（= `ambient_light_provider` 槽参数块）的**职责边界与规则**；
2. 把 `env_tex` 从句柄寄生的位置**归位到它的消费者槽**，并借此**消灭 `_pad[3]` 与偏移类断言**（治根而非兜底）；
3. 参数块纳入世界表地址体系（每块一个 `addr_*`、每块一个写者，与 `addr_lights` 同形，见 I4）；
4. 明确"逐帧演化的天空方法"对帧槽口径的要求（单份 → 按帧槽分份的升级条件）；
5. 给出结构镜像对拍的**通用**机制方向（替代"一个结构一条专用门"）。

**非目标（明确不做，防止架构走死）**
- ❌ 实现任何天空/天光方法（真大气散射 / 程序化天空 / HDRI / 浑浊度与臭氧模型 …）：那些是**方法实现**，另立项；
- ❌ cluster / tiled / deferred、GI/RT 脚手架；
- ❌ 为第二个 ambient provider / 第二种天空方法预留结构（I3）；
- ❌ 改 CSM、光源行、影子域（T6a/T6b 已定型）；
- ❌ 现在给 `SkyInfo` 写专用门（见 §7 变更记录：本稿推翻了上一轮的建议）。

---

## 2. 目标形态

### 2.1 四条规则（新增天空/环境光方法的准入条件）

- **R1 —— 一个 provider 槽 = 一个参数块 + 一个世界表地址。**
  `SkyInfo` 只承载"当前被选中的 `sky_provider` 槽的共用参数"；其它槽（`ambient_light_provider` 等）的参数
  **不许再借住**。新增天空方法 = 新增它自己的块 + 自己的 `addr_*`（照 `addr_lights` 的做法：块/行表 + GLSL 镜像 +
  世界表下发 + 结构对拍），**不是**往 `SkyInfo` 里堆字段。
  *理由*：现状已是一一映射（§0.3 表）；堆在一个结构里，未来 N 种天空方法就是 N 组互不相干的字段共用一份 ABI 契约，
  每加一个方法都要动全局结构 + `_pad` + 断言 + 门 —— 这次 IBL 事故就是这个形态的税。

- **R2 —— 块内只放同族字段；句柄类单独成块。**
  外观标量块（颜色/强度/角度…）与句柄块（bindless 句柄、纹理引用）不混排。`_pad` + `offsetof` 断言是**兜底**，
  不是解法：只要块内字段同族（同对齐要求），两端布局就不可能因"隐式对齐"分歧（I5）。

- **R3 —— 逐帧演化的参数块必须按帧槽分份。**
  单份 buffer（全帧槽同址）只在"谁都不逐帧改它"时安全。某个天空方法一旦随时间演化（程序化天空/时刻/天气），
  它必须像光源行那样**按帧槽各一份**（地址 = 基址 + 帧槽 × `sizeof(块)`），否则在飞帧会读到下一帧的值。
  **升级触发条件**：该方法第一次在 `Tick` 之类逐帧路径上写参数。

- **R4 —— 结构对拍做成通用机制，不做专用门。**
  目标形态：C++ 侧导出结构字段清单（名/类型/偏移），GLSL 侧声明同一张表，**门比对两者**；
  一次覆盖 `LightRow` / `ShadowInfo` / `SkyInfo` / `AmbientEnvInfo` / 未来每个块。
  （在它落地之前，`static_assert` + I5 纪律是唯一护栏。）

### 2.2 搬迁后的块形态（本稿的执行目标）

| 块 | 内容 | 地址 | 写者 | 帧槽 |
|---|---|---|---|---|
| ~~`SkyInfo`（`sky_provider`）~~ **已整删（2026-10-07）** | 两个外观标量 ⇒ 取值搬成 `sky_atmosphere.glsl` 常量 `kBaseSkyColor` / `kHaloIntensity`（**外观逐位不变**） | ~~`addr_sky`~~（一并删除） | ~~`EnvironmentSystem::EditSkyInfo`~~（一并删除） | —（无块则无帧槽问题；将来天空方法需要参数时按 R1 建**它自己的**块） |
| `AmbientEnvInfo`（`ambient_light_provider`） | `Vector4u env_tex`（3 句柄 + 1 预留，16 B，块内只有句柄 ⇒ 天然 16 对齐） | **新增** `addr_ambient_env` | `EnvironmentSystem`（或新 `AmbientLightSystem`，见下）| 单份（同 R3） |

> 写者归属：`env_tex` 的写者是**示例/资源侧**（`IBLEnvironment` 注册三张纹理后写句柄）。
> 最小改动 = 提供一个 `EnvironmentSystem::EditAmbientEnvInfo()`（与 `EditSkyInfo` 同形，同一 profile 生命周期）。
> **不要**为它新建一个系统 —— 目前它没有逐帧逻辑，只有"资源就绪时写一次"（I3/S2：不为未实现的机制造系统）。

---

## 3. 接线面

### 3.1 新增文件

| 文件 | 内容 |
|---|---|
| `inc/hgl/graph/ubo/AmbientEnvInfo.h` | `AmbientEnvInfo` POD（`Vector4u env_tex` + 预留）+ `static_assert(sizeof == 16)` + `offsetof(env_tex) == 0` |
| `ShaderLibrary/ubo/sky_provider_params.glsl`（或并入 `scene_ubo.glsl`，见下） | GLSL 镜像（`struct AmbientEnvInfoRef` + `#define ambient_env AmbientEnvInfoRef(world_addresses.addr_ambient_env)`） |
| `src/ecs/support/TestCSMIncrementalPass.cpp` 内新增 **Test 47** | 契约（见 §5） |

> ⚠ **GLSL 侧落点二选一**（执行时决定，两者都不新增"模块"则不欠门的模块计数）：
> (a) 并入现有 `ShaderLibrary/ubo/scene_ubo.glsl`（与 `SkyInfoRef`/`WorldAddresses` 同处）；
> (b) 单开一个 `ubo/` 模块 —— **若新增独立模块，门的"模块总数"三处必须同批 +1**（见 §6 R3）。

### 3.2 改动触点（逐处缺一不可）

| 文件 | 改动 | 为什么缺一不可 |
|---|---|---|
| `inc/hgl/graph/ubo/AmbientEnvInfo.h` | 新建块 | —— |
| `inc/hgl/graph/ubo/SkyInfo.h` | 删 `env_tex`、删 `_pad[3]`、删 `env_tex@32` 与 `sizeof==48` 两条断言（改 `sizeof==20`） | 不删 = 双真源（I5 的病灶） |
| `inc/hgl/graph/ubo/WorldAddresses.h` | 加 `uint64_t addr_ambient_env`（第 10 字段，72 → 80 B，仍在 128 B 槽步长内） | I2/I4 |
| `src/ecs/support/TestRenderItemDataStorage.cpp` | `sizeof(WorldAddresses)` 断言 72 → **80** + 新增 `offsetof(addr_ambient_env) == 72` + 零值初始化检查加一项（另有内嵌夹具，`grep -n "sizeof(graph::WorldAddresses)" src/` 全部同步） | 夹具不同步 = 契约测试红 |
| `ShaderLibrary/ubo/scene_ubo.glsl` | `WorldAddresses` 镜像块加 `uint64_t addr_ambient_env;` + `AmbientEnvInfoRef` + `#define ambient_env ...`；`SkyInfoRef` 删 `uvec4 env_tex` | 两端镜像不同步 = 静默读垃圾（I5） |
| `ShaderLibrary/lighting/indirect_ibl.glsl` | 3 处 `sky.env_tex` → `ambient_env.env_tex` | 唯一读者 |
| `inc/hgl/ecs/systems/render/EnvironmentSystem.h/.cpp` | 加 `EditAmbientEnvInfo()/GetAmbientEnvInfo()/SetAmbientEnvInfo()` + `MarkAmbientEnvDirty()`（与 sky 三件套同形） | 写者入口 |
| `inc/hgl/graph/ubo/EnvironmentInfo.h` | `EnvironmentInfo` 加 `AmbientEnvInfo ambient_env;` | profile 承载（与 `sky` 同级） |
| `inc/hgl/graph/module/EnvironmentManager.h/.cpp` | `Profile` 加 `ambient_env_ubo` + `MaterializeAmbientEnvUBO` + `GetAmbientEnvAddress` + `MarkDirty` 覆盖 + `EnsureDefault` 立即物化 | profile 侧物化与取址（照 `sky_ubo` 三件套） |
| `src/ecs/core/Context.cpp` | `SyncWorldAddresses` 发布 `want.addr_ambient_env` | I4：地址必须接发布 |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | `kAddressOwnershipTable` 加 `{ "addr_ambient_env", true }`（世界私有） | 归属门 |
| `example/Environment/IBLEnvironment.cpp` | `EditSkyInfo()` → `EditAmbientEnvInfo()` 写三句柄 | 真实消费者 |
| 契约测试 | Test 47 +（可选）Test 45 追加"`sky.env_tex` 禁复活" | 防回潮 |

---

## 4. 分阶段实施

> 每阶段：内容 / 判据 / 回滚点。**T3/T4 是触发项**（条件不满足就不做，见 S2/I3）。

### T0 —— 判据前置：给两个消费者示例接帧摘要（建议第一步）

- **为什么先做**：八台架覆盖天空**外观标量**（经默认模板的 ambient），但 **`env_tex`/IBL 与"天空球自身外观"两条路径
  都在盲区**（§0.3）；不先补判据就改这块，等于又一次只能靠肉眼验收。
- **内容**：照 `example/Shadow/CascadeShadowMap.cpp` 的 `CSM_SELFCHECK` 模式（`example/Shared/FrameDigest.h`：
  `InstallFrameDigestHook(rt, expect, "<TAG>")`），给
  `example/Environment/IBLEnvironment.cpp` 与 `example/Environment/AtmosphereSkyMinimal.cpp` 各加
  `<PREFIX>_SELFCHECK=1` 自检：冻结静止场景（从第 0 帧起）、第 8 帧帧内回读颜色靶、比对 `{distinct_byte_values, fnv1a64}` 常量，
  PASS 退出 0 / FAIL 退出 1。常量由**当前 HEAD 实跑记录**（这就是改结构前的基线）。
  ⚠ 分辨率必须与窗口一致（摘要算在交换链图像上，尺寸一变数值全不可比）。
- **判据**：两个示例 `<PREFIX>_SELFCHECK=1` rc=0，且记录的摘要 ≠ 0、`distinct` ≥ 2（不空转）。
- **回滚点**：tag `pre-sky-t0`（纯加法，独立于结构改造）。

### T1 —— 块归位（单批：加法 + 减法同批，见 P1）【**已落地 2026-10-07**，见 §5 的 T1 实跑记录】

- **内容**：按 §3 触点表一次做完 —— 新建 `AmbientEnvInfo` + `addr_ambient_env` + GLSL 镜像与宏 +
  profile 侧物化/取址 + 发布 + 写者入口 + 示例改写 + `SkyInfo` 删 `env_tex`/`_pad`/两条断言 + 门归属登记 +
  Test 47 + 夹具同步。
- **判据**：
  1. **I7 全量重编**：清整棵 `*.dir/Debug` + 两个 shader-cache 后构建，`BUILD_RC=0` / 0 error（命令见 §8）；
  2. 门 **46 PASS / 0 FAIL**（若 §3.1 选了"独立 GLSL 模块"，则应为 **47 PASS / 0 FAIL**）；
  3. `TestCSMIncrementalPass` rc=0（Test 47 Passed）；`TestRenderItemDataStorage` rc=0（80 B 夹具过）；
  4. 八台架 + RTT/RTTCD 摘要**逐位不变**（它们不消费 sky/IBL ⇒ 期望"零变化"，正好用来证明"没误伤"）；
  5. **T0 的两条自检 rc=0**（这才是真正的目标判据：外观逐位不变 + 无 VUID/ERROR）；
  6. `grep -rn "sky\.env_tex\|_pad\[3\]" ShaderLibrary/ inc/hgl/graph/ubo/SkyInfo.h` 零命中。
- **回滚点**：tag `pre-sky-t1`（动全局结构 ⇒ 本阶段起是**不可逆点的开始**；按 P1 单批则整批 revert）。

### T2（触发）—— 帧槽分份（R3）

- **触发条件**：某个天空/ambient 方法第一次在逐帧路径上写参数（程序化天空随时间/天气演化等）。
- **内容**：把该块从"单份"改为"按帧槽各一份"（照 `addr_lights`/影子数据的做法：步长 = `sizeof(块)`，
  只写当前交换链帧那一槽、且**在 acquire 之后**写），`SyncWorldAddresses` 发布 `基址 + 帧槽 × 步长`，
  写者改为"提交点写本帧槽"（照 `ECSContext::CommitShadowInfo`）。
- **判据**：八台架摘要仍逐位 + 该方法的自检 + 新增契约（「步长口径/写本帧槽/离 acquire 之前不写」各一条）。
- **回滚点**：tag `pre-sky-t2`。
- **⚠ 2026-10-07：触发面已缩小** —— `sky` 段整删后，本任务只剩 `AmbientEnvInfo` 一段（16 B）可被逐帧演化。
  另按上一轮口径（会话内已拍板）：**逐帧演化的参数块 = 世界私有 + 按帧槽分份** —— 只做分份会在
  profile 侧把「跨帧覆写」换成「跨世界覆写」（共享 profile 的两个世界同槽互踩），所以 T2 落地时
  建议与世界私有化同批。

### T3（触发）—— 通用结构镜像对拍（R4）

- **触发条件**（2026-10-07 重新口径，用户拍板）：原写「第 3 个 BDA 结构块出现，或布局错位事故再次发生」，
  但**按块数判这条已失去筛选力**——今天仓库里已有 5+ 个 BDA 结构块（`CameraInfo` / `LightRow` / `ShadowInfo` /
  `SkyInfo` / `AmbientEnvInfo` / `EntityGPU` / `RenderItemDescriptor` …），且 2026-10-07 刚发生过一次布局错位（I5）。
  ⇒ 改为**按需求触发**：**新增一个 provider 参数块 / 新增一个 BDA 结构块时**，把该块一并纳入 manifest 比对
  （T3 与新块同批落地）；**不为存量块补专门门**（它们今天由 `W.*-struct-parity` 那几条既有用例 + `static_assert` 覆盖）。
- **内容**：定义"字段清单 manifest"格式（`{name, glsl_type, offset}`），C++ 侧由宏/脚本导出，
  GLSL 侧要求在结构前声明同一张清单（注释块），门新增一例通用比对（替代"每结构一条"）。
- **判据**：故意改一个字段类型/顺序 ⇒ 门 FAIL；复原 ⇒ PASS。门 PASS 数按实现方式 +1。
- **回滚点**：tag `pre-sky-t3`。

### T4（触发）—— 第二个 ambient provider / 第二种天空方法

- **触发条件**：真实需求出现（第二天空方法、第二个 ambient provider、或多世界各用不同方法）。
- **内容**：此时才把 P2(b)「provider 行表」与 P5「profile ↔ provider 选择」定案；结构按 R1 新增各自的块。
- **判据**：按新建时的矩阵定。

---

## 5. 验证矩阵

| # | target | 判据 |
|---|---|---|
| V1 | 门 `ShaderResourceSchemaRegressionGate` | **46 PASS / 0 FAIL**（新增独立 GLSL 模块则 47；新增地址字段改 `kAddressOwnershipTable` 不改 PASS 数） |
| V2 | `TestCSMIncrementalPass` | **rc=0**，46 个测试 + 新增 Test 47（Test 22 = 25 条两世界隔离；44/45/46 = T6b 契约） |
| V3 | `TestRenderItemDataStorage` | **rc=0**（`WorldAddresses` 80 B 夹具） |
| V4 | 八台架（PBRSpheres / ShadowMap / CascadeShadowMap / GPUDrivenCull / AlphaModes / CoverageVariants / LoadScene / PlaneGrid3D） | `<前缀>_SELFCHECK=1 FRAMEDIGEST_PERFRAME=9`：摘要**逐位不变** — `0x3DD88A0999CDC486` / `0xA8A369516CB04571` / `0x1CDAADEBF18B7CE4` / `0x90E0745C2C7B8599` / `0x2FE87B15EE8B8A16` / `0x8EED38BAD9B8181E` / `0xC9E38ED2F49A069B` / `0x4125C3C830CB92D3`（Cascade 为 1600×900） |
| V5 | `RenderToTexture` / `RenderToTextureColorDepth` | `0xBA9CA214778AF3A4` / `0xD71ABABC2BE460C0`（逐位） |
| V6 | `AlphaTestShadow` | `ATS_SELFCHECK=1`：D1 57.6%（bbox 112×58）/ D3 18187 & 19109 px / bias 600662 px / D4 拒绝 0 / rc=0 |
| V7 | **环境示例族 5 个**（IBLEnvironment / BasicLitSunDirection / AtmosphereSkyMinimal / AtmosphereSkyAmbient / AtmosphereSkySunGizmo） | T0 之前：0 VUID / 0 `[ERROR]` / 0 shader 编译失败（**非数值判据**）；**T0 之后**：IBLEnvironment 与 AtmosphereSkyMinimal 的 `<PREFIX>_SELFCHECK=1` rc=0（基线常量由 T0 实跑录入本档） |
| V8 | 环境示例族目视 | 强制项：IBLEnvironment 里球/环/方**受光且显示环境反射**（不是纯黑 + 单个高光点 —— 这是 I5 的典型症状） |

**T0 录入位（执行时把实跑值填进来）**：

| 示例 | 自检环境变量 | 分辨率 | distinct_byte_values | fnv1a64 |
|---|---|---|---|---|
| IBLEnvironment | `IBLENV_SELFCHECK=1` | 1280×720 | 256 | `0x2D28D72AEA4FC87D` |
| AtmosphereSkyMinimal | `SKYMIN_SELFCHECK=1` | 1280×720 | 219 | `0xC914C338EFC7790E` |

> 上表 = **T0 实跑值**（改结构前的基线；帧 8、帧内回读、A2BGR10UN 打包字节）。
> 逐帧自证（`FRAMEDIGEST_PERFRAME=9`）：两例 9 帧 × 3 张交换链图**逐位同哈希**、`与首帧不同的帧数=0`
> ⇒ 静止场景下摘要确定（不是「每帧闪一帧」被采样避开）。
> 改渲染结果（少画 / 错画 / 挪相机 / 句柄错位）⇒ 摘要必变，常量必须复核重录。
> 两条自检已并入技能脚本 `ulre-gpu-pipeline-verification/scripts/full_regression_criteria.py` 的
> **ENV 组**（它们不打 `ENGAGED` ⇒ 单走 rc + 摘要 + `D=0` 判据）。

**T1 实跑记录（2026-10-07，已落地）**：单批完成 §3.2 触点表全部 13 处。

| 判据 | 实测 |
|---|---|
| I7 全量重编（清整棵 `*.dir/Debug` + 两个 shader-cache） | `BUILD_RC=0` / 0 error |
| 门 `ShaderResourceSchemaRegressionGate` | **46 PASS / 0 FAIL**（并入 `scene_ubo.glsl`，模块总数未变 ⇒ 不欠门） |
| `TestCSMIncrementalPass` | rc=0（**Test 47 Passed**；Test 45 的「句柄在天空镜像里」needle 同批改指新块） |
| `TestRenderItemDataStorage` | rc=0（`WorldAddresses` 80 B 夹具；3 处内联 GLSL 镜像同批加字段） |
| 八台架 + 离屏两例（12 项，脚本全跑） | 摘要**逐位不变**、`ENGAGED x==y`、guard 拒绝=0 |
| **T0 两条自检（真正的目标判据）** | 逐位不变：IBLEnvironment `0x2D28D72AEA4FC87D` / AtmosphereSkyMinimal `0xC914C338EFC7790E`，`D=0` |
| 环境示例族 5 例 | 0 VUID / 0 `[ERROR]` / 0 shader 编译失败 |
| V8 目视（IBLEnvironment） | 球/环/方**受光 + 显示环境反射**（非「纯黑 + 单个高光点」= I5 症状） |
| 判据 6 零残留 | `grep -rn "sky\.env_tex\|_pad\[3\]" ShaderLibrary/ inc/hgl/graph/ubo/SkyInfo.h` 零命中 |
| 全仓 `env_tex` 残留 | 只剩新块 / 新镜像 / 唯一消费者 / 示例 / Test 47 针（无历史形态） |

> ⚠ 与 §2.2／§3.2 的一处口径订正：`SkyInfo` 实际 `sizeof` = **32**（20 B 有效数据 + 12 B 尾部对齐填充）
> —— 本工程在 `CMCMakeModule/math.cmake` 定义 `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` ⇒ `Color4f`（glm::vec4 派生）
> 16 对齐 ⇒ 结构体尺寸向上取整。**不是** 20。尾部填充其后没有字段 ⇒ 两端不构成契约，
> 真正的契约只有偏移 `base_sky_color@0` / `halo_intensity@16`（断言按实际值写成 32）。

### 5.1 场景 A 实跑记录（2026-10-07：sky 段整删）

**改动面**（一次落地）：`SkyInfo.h` 删除；`EnvironmentInfo` 只剩 `ambient_env`；`WorldAddresses` 删 `addr_sky`
（80 → **72 B**，夹具 `sizeof/offsetof/零值` + 3 处内联 GLSL 镜像 + 门 `kAddressOwnershipTable` 同步）；
`scene_ubo.glsl` 删 `SkyInfoRef` + `#define sky` + 世界表镜像字段；`Context.cpp` 删 sky 地址发布与日志字段；
`EnvironmentManager` 删 `sky_ubo` / ``MaterializeSkyUBO`/ `GetSkyAddress` / `MarkDirty` 的 sky 一半 / 释放路径；
`EnvironmentSystem` 删 `EditSkyInfo`/`GetSkyInfo`/`SetSkyInfo`/`MarkSkyDirty`；两处 CMake 源清单（SceneGraph/ShaderGen）同步；
shader 两个标量 → 常量 `kBaseSkyColor` / `kHaloIntensity`。
**纪律**：结构尺寸变了 ⇒ 清整棵 `*.dir` obj 树 + 清全部 `shader-cache` 后重编（I7）。

| 判据 | 实测 |
|---|---|
| 全量重编 | 0 编译错误（3 个 CM 示例的 `MSB3073` 来自 vcpkg `z-applocal` 并行拷贝竞态；单建各目标 RC=0） |
| 门 `ShaderResourceSchemaRegressionGate` | **46 PASS / 0 FAIL**（`addr_sky` 归属登记删掉不改 PASS 数） |
| `TestCSMIncrementalPass` | rc=0（Test 44/45/46/47/48 全 Passed；Test 47 新增 ⑥「sky 整删」11 条） |
| `TestRenderItemDataStorage` | rc=0（`WorldAddresses` **72 B** 夹具 + 3 处内联 GLSL 镜像同步） |
| 八台架 + 离屏两例（12 项） | **全 PASS**，摘要**逐位不变**、`D=0`、`ENGAGED x==y`、拒绝 0 |
| **环境两条自检（外观零变化的实证）** | **逐位不变**：IBLEnvironment `0x2D28D72AEA4FC87D` / AtmosphereSkyMinimal `0xC914C338EFC7790E` |
| `AlphaTestShadow` | rc=0，拒绝 0 |
| V7 `DualWorldShadows` | **PASS**（S0=S1=`0x55B0F81FEB86BACC` / S2=`0xD58FA9C3DF405ACF`；子世界 S0=`0xB63D9FE07F9BEA9C` / S1=S2=`0xFDA63532CCBB8E2F`，与 C4 记录一致） |
| 环境示例族 5 例 | 0 VUID / 0 `[ERROR]` / 0 shader 编译失败 |

> 结论：**外观逐位不变**（两条自检 + 八台架摘要同时不变 = 结构性删除没有动渲染结果），且 R1–R4 规则未变。
> 这是"删死代码"的标准收口姿势：**先证明"没有人写它"，再证明"删了画面一样"**。

### 5.2 世界表地址列「0 地址口径」收口（2026-10-07，场景 A 后续小批）

**问题**（本稿 §7 记录的那一条）：`SyncWorldAddresses` 里各地址列的失败语义**互不一致**——
`addr_l2w` 用 `(addr != 0) ? addr : dst->addr_l2w`（保留上一份好值），而 `addr_ambient_env` 是
`if (addr != 0)` 的"静默丢值"写法（`want{}` 从零起算 ⇒ **等价于把 0 写进表**，与注释完全相反），
影子/实体行/4-ID/光源行四列更是**无守卫**地把 `slot × 步长` 加到基址上（未建表时写出 `slot×832` 这类
"看着有效"的垃圾地址 ⇒ 消费者判不出缺席 ⇒ 解引用 UB）。

**处置**（纯一致性，不改正常路径行为）：`Context.cpp` 抽出两个单点 ——
`pick(resolved, previous)`（0 ⇒ 沿用本槽旧值）与 `slot_offset(base, per_slot_bytes)`（基址 0 ⇒ 不参与
slot 偏移），**全表九列**统一走它们；注释与实现同批改正（`addr_l2w` 作为口径的首个使用者一并并入）。

| 判据 | 实测 |
|---|---|
| 编译 | `Context.cpp` 重编 rc=0 / 0 error（**结构未变 ⇒ 无需清树**） |
| `TestCSMIncrementalPass` | rc=0（新增 Test 47 ⑦ **12 条**：`pick`/`slot_offset` 单点 2 条 + 九列逐条 9 条 + 静默丢值/无守卫写法**禁复活** 2 条；同批更新 Test 22 相机列、Test 46 ⑤ 光源列与影子步长三条既有 needle） |
| 门 | **46 PASS / 0 FAIL** |
| 八台架 + 离屏两例 + 环境两条 + ATS | **ALL PASS**，摘要**逐位不变**、`ENGAGED x==y`、拒绝 0 |
| V7 `DualWorldShadows` | **PASS** |

> 附带价值：这套口径把"未建表"与"解不到"都收敛成**同一条可判据的不变量**，
> 且**结构性防住**了本轮 `sky` 事故的同类变体（垃圾地址比 0 更坏：消费者连"缺席"都判不出来）。

---

## 6. 风险

| # | 风险 | 默认选择 |
|---|---|---|
| R1 | 布局错位再犯（I5/R2） | 块内只放同族字段；句柄块独立；改字段后**清 obj + 清 shader-cache 全量重编**；目视 + T0 自检双判据 |
| R2 | 只在"结构改造"上做对拍、却漏了 profile/发布/夹具同批 | 按 §3.2 触点表逐项勾；跑门 + 两个契约测试（I2/I4 的载体） |
| R3 | 新增 GLSL 模块时漏了门的"模块总数"三处同步 | 优先并入 `scene_ubo.glsl`（不新增模块）；确要新增则三处同批 +1 并确认 PASS 数 +1 |
| R4 | `addr_ambient_env` 只有字段没写者/没发布（I4 的历史坑） | T1 时**同时**接写者与发布，并在实跑里确认该地址非 0 |
| R5 | 单份 buffer 遇上逐帧演化（R3） | 触发即升级为按帧槽分份（T2）；不要"先在单份上试"，那是在飞帧读错值的经典来源 |
| R6 | 把"归位"扩大成"天空系统重构" | 本稿只动**数据面**（块/地址/写者）；方法实现与非目标一律不做 |

---

## 7. 变更记录

| 旧说法（出处） | 新结论 | 原因 |
|---|---|---|
| 「给 `SkyInfo` 加一条专用门 `W.sky-info-struct-parity`」（本会话上一轮的助手建议） | **不做专用门**；改为 R2（块内同族字段 + 句柄独立成块）治根 + T3 的**通用**对拍机制 | 结构未定型、只有 2 个消费者、48 B；专用门治的是症状（偏移对齐）。真正病根是"句柄寄生在外观结构里"（§0.3 IBL 事故） |
| 「`SkyInfo` 是光照参数结构」（T6b 之前的形态） | `SkyInfo` = **`sky_provider` 槽的参数块**；光源参数在光源行、IBL 句柄归 `ambient_light_provider` | 字段与消费者的映射本就是一槽一块（§0.3 表） |
| 「T1 后 `sizeof(SkyInfo) == 20`」（本稿 §3.2 原写法） | 实测 **32**（20 B 数据 + 12 B 尾部对齐填充），断言按 32 落地 | 工程定义 `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` ⇒ `Color4f` 16 对齐 ⇒ 尺寸向上取整；**写断言前先量化**（否则又一条「凭形状推断」的错断言） |
| 「`SkyInfo` 作为 `sky_provider` 参数块保留（本稿 §2.2 原写法）」 | **整删**（`SkyInfo.h` / `addr_sky` / GLSL 镜像与宏 / 编辑入口一并删） | 两个外观字段全仓**无写者**、只服务一种假大气方法 ⇒ 搬成 shader 常量后外观逐位不变；空结构 + 从未被写过的地址没有保留价值（零兼容、不留空壳）。**留着反而违反 R1**：它会招「往天空块堆字段」的老路 |
| 「取不到地址（0）时保留上一份好值」（`SyncWorldAddresses` 的注释） | **注释与实现不符**：`if (addr != 0)` 的「静默丢值」写法在 `want{}` 从零起算下等价于**把 0 写进表**；影子列更甚（无守卫 ⇒ 未建表时写出 `slot×832` 垃圾地址）。 | **✅ 已收口（2026-10-07，场景 A 后续小批）**：全表**九列**统一为 `pick(解出值, 本槽旧值)` + `slot_offset(基址, 步长)`（基址 0 不参与 slot 偏移），注释与实现同批改正、Test 47 ⑦ 逐列钉住。见 §5.2 |
| 「T3 触发 = 第 3 个 BDA 结构块出现」（本稿 §4 原写法） | 改为**按新需求触发**：新增参数块 / 新增 BDA 结构块时随该块一起落地；存量块不补专门门 | 按块数判在今天就已成立（5+ 块且刚出过一次布局错位）⇒ 条件失去筛选力，是「任何时刻都成立 = 永远不做」的伪触发 |

---

## 8. 附录

### 8.1 关键文件索引

- 天空/环境数据：`inc/hgl/graph/ubo/{AmbientEnvInfo.h, EnvironmentInfo.h, WorldAddresses.h}`
  （`SkyInfo.h` 已于 2026-10-07 整删；天空外观常量在 `ShaderLibrary/sky/sky_atmosphere.glsl`）
- profile 管理：`inc/hgl/graph/module/EnvironmentManager.h` + `src/SceneGraph/module/EnvironmentManager.cpp`
- 系统与帧入口：`inc/hgl/ecs/systems/render/EnvironmentSystem.h/.cpp`、
  `src/ecs/core/Context.cpp`（`SyncWorldAddresses` / `Initialize*Table` / `CommitShadowInfo`）
- shader：`ShaderLibrary/ubo/scene_ubo.glsl`（`WorldAddresses` 镜像 / `AmbientEnvInfoRef` / `#define ambient_env`；
  `SkyInfoRef` 与 `#define sky` 已于 2026-10-07 删除）、
  `ShaderLibrary/sky/sky_atmosphere.glsl`（`sky_provider`）、`ShaderLibrary/lighting/indirect_ibl.glsl`（`ambient_light_provider`）
- 消费者示例：`example/Environment/{IBLEnvironment.cpp, AtmosphereSkyMinimal.cpp}`
- 判据载体：`example/Shared/FrameDigest.h`、`src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp`、
  `src/ecs/support/{TestCSMIncrementalPass.cpp, TestRenderItemDataStorage.cpp}`

### 8.2 前序状态（本稿的起点，T6b 已结）

- T6a 影子域搬运 `e355f0da2`；T6b 加法 `10edaf040` + `4d53abb34` + `b576daa7a`；
  T6b 减法 `dc49c0dd9` + `24cfb90e6`；`SkyInfo` 布局错位修复 `949234482`；P4 影子世界私有化 `ada3fa48b` + `b46fced20`。
- 已定型：光源行（`LightRow` 64 B × 16 行，`addr_lights` 按帧槽切份，行 0 = 主方向光，`LightSystem` 写）；
  影子数据（世界私有，`ECSContext::shadow_info` + `InitializeShadowInfoTable` + `CommitShadowInfo`，`addr_shadow`）。
- `SkyInfo` 现状 = **已整删（2026-10-07）**；当时形态为 `base_sky_color` + `halo_intensity` + `_pad[3]` + `env_tex`（48 B）。

### 8.3 执行须知（给新会话）

**环境**：仓库在 `E:\ULRE` 或 `D:\ULRE`（**路径随机器**；文档里出现的 E:/ 一律读作当前机器仓库根）。
Windows + bash（MSYS）：`terminal` 里用 POSIX 语法，给原生程序传 `C:/...` 正斜杠路径。

**命令模板**

```bash
cd /e/ULRE
# ① 改结构体后必做：杀掉构建、清 obj 树、清 shader-cache（I7）
taskkill /F /IM MSBuild.exe
rm -rf build/out/Windows_64_Debug/shader-cache build/cache-hot/shader-cache
find build -type d -path "*.dir/Debug" -print0 | xargs -0 rm -rf
# ② 构建（判据用到的目标集；全量 build 里 `minipack_info` 会因 vcpkg z-applocal 失败，见下）
cmake --build build --config Debug --target ULRE.ECS ULRE.SceneGraph TestCSMIncrementalPass \
  TestRenderItemDataStorage ShaderResourceSchemaRegressionGate IBLEnvironment AtmosphereSkyMinimal \
  PBRSpheres ShadowMap -- -m:8 -v:m
# ③ 门 / 契约
./build/out/Windows_64_Debug/ShaderResourceSchemaRegressionGate.exe   # 期望 46 PASS / 0 FAIL
./build/out/Windows_64_Debug/TestCSMIncrementalPass.exe; echo rc=$?    # 期望 rc=0
./build/out/Windows_64_Debug/TestRenderItemDataStorage.exe; echo rc=$? # 期望 rc=0
# ④ 摘要台架（窗口程序：跑完必须 taskkill）
env PBR_SELFCHECK=1 FRAMEDIGEST_PERFRAME=9 ./build/out/Windows_64_Debug/PBRSpheres.exe
taskkill /F /IM PBRSpheres.exe
```

**已知坑（都踩过）**

1. **不要用 `| grep error` 判构建结果**：`grep -c` 零匹配返回 1 ⇒ 假红。判据取日志里的 `BUILD_RC=` 行与错误计数。
2. **全量 `cmake --build build` 里 `minipack_info` 会卡在 vcpkg `z-applocal` 后置 DLL 部署步**
   （`MSB3073`/exit 32）：**编译侧 0 error**，纯工具链环境问题、与该目标的功能无关（用户口径：minipack 能正常用，
   不必管）⇒ 判据一律用上面的目标集构建。（原并列的 `DrawCurvesTest` 已于 2026-10-07 从 CM2D 删除。）
3. 跑判据时先 `taskkill` 占用 exe 的旧进程，否则链接报 `LNK1104 cannot open file`。
4. 别用 `shell=True` + 相对路径跑 exe 判退出码；契约失败的编号是**精确**的（如 Test 44 失败 ⇒ rc=44）。
5. 改完 `.glsl` **不需要重编 C++**（运行时编译），但要清 shader-cache 才能确保不读旧缓存。

**编辑纪律**：`doc/*.md` 与源码按原文件 CRLF/BOM 形态往返（本仓 doc = CRLF 无 BOM；部分源码 = BOM+CRLF）；
脚本里拼多行替换串必须先 `.replace('\n','\r\n')`；写完校验 `data.count(b'\n') == data.count(b'\r\n')`。

**相关技能**：`ulre-ecs-layer`（系统/阶段/拆域搬运清单）、`ulre-ssbo-row-plumbing`（行空间/ABI 对拍与句柄错位诊断）、
`ulre-contract-tests`（契约测试写法）、`crlf-safe-editing`、`ulre-gpu-pipeline-verification`、`ulre-design-plan-authoring`。
