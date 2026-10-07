# 光影域拆分与光源数据模型方案（Environment / Light / MainLightShadow 三域）

> **版次**：v1（2026-10-07）。范围 = **拆域 + 光源数据模型**；**不含**多光源渲染机制、cluster/deferred、
> GI/RT 脚手架（非目标见 §1）。本稿由用户拍板的三条决策（§0.1）定形，落地走 **T6a → T6b** 两步。
>
> **关联文档**：`doc/csm-mechanism.md`（CSM 机制本身，本稿不动其数学/缓存策略）、
> `doc/world-addresses-and-camera-model-plan.md`（地址三档；本稿新增 `addr_lights` 与它同级）、
> `doc/ENVIRONMENT_SYSTEM.md`（本稿**改变其一部分结论**：Environment 不再托管阴影、不再持有光源参数）、
> `doc/shadow-component-and-automated-pipeline-design.md`（本稿**推翻其 §1 的一句定论**，见 §8 变更记录）、
> `doc/shadow-mask-and-hybrid-shadows-design.md`（**未来多光源阴影**的设计草案 = 本稿的消费者）。

---

## 0. 设计输入

### 0.1 已定输入 —— 用户决策（2026-10-07）

| # | 决策 | 落点 |
|---|---|---|
| D1 | **走 B 方案**：拆成三域（Environment / Light / MainLightShadow），**分两步提交**（T6a 纯搬运 → T6b 数据模型） | §3 / §5 |
| D2 | **光源数据 = 世界私有独立表**（`addr_lights`，与 `addr_entity_gpu` 同级），**不跟 `EnvProfile`** | §2 |
| D3 | `SkyInfo.moon_color` / `moon_intensity` **删除**（零兼容；真需要第二光源时再加行） | §4 触点 |
| D4 | **不**新增"大而全光影系统"；**不**为 GI/RT 造脚手架（加速结构/降噪/帧间历史 = 另立项） | §1 非目标 |
| D5 | **P1–P4 按推荐拍板**（2026-10-07）：容量 16 / `alignas(64)` / controller 移 `render/shadow/` / `ShadowInfo` 世界私有 | §9 / §2 / §4 |

### 0.2 已定输入 —— 引擎不变量（违反即编译失败或渲染崩）

| # | 不变量 | 出处 |
|---|---|---|
| I1 | 地址三档：`GlobalAddresses`（跨世界池）/ `WorldAddresses`（世界私有）/ `pc_root`（每批）；shader 经 BDA 取数，**描述符集只剩 `pc_root` + Bindless=0** | `inc/hgl/graph/ubo/WorldAddresses.h`、`inc/hgl/common/DescriptorSetTypeDef.h` |
| I2 | 世界私有表当前有：camera_info / global_render_items / draw_item_ids / **sky** / **shadow** / entity_gpu / l2w / entity_render_ids … | 同上 |
| I3 | **容量冻结 + 超限 fail-fast**（不扩容、不静默截断）；新增枚举必须 **constexpr 全表覆盖**判定（禁止逐项手写断言） | 用户既定纪律 |
| I4 | 新地址字段**必须接写者**（`addr_l2w` 曾只有字段没写者 ⇒ 静默剔空） | `ulre-entity-gpu-cull` 约定 10 |
| I5 | CSM：**每级联一个离屏 RT + 一条离屏数据槽带**（`OffscreenSlotBands` first-fit + Destroy 归还）；光相机槽由 `CameraSlotGuard` RAII 持有 | `doc/csm-mechanism.md` §5/§8、`inc/hgl/ecs/support/CameraSlotGuard.h` |
| I6 | 帧槽空间：主帧 `[0, HGL_FRAME_SLOT_MAIN=4)` / 离屏 `[4, HGL_FRAME_SLOT_TOTAL=8)` 按 RT **整带**；per-frame ring 深度必须 **== 槽空间** | `inc/hgl/common/RenderOptions.h`、`ulre-frame-sync-slots` |
| I7 | 结构类值（批标签/切片/计数）必须与绘制同源；**改结构体 ⇒ 清 obj 树全量重编** | 同上 |
| I8 | 判据基线：门 **45 PASS / 0 FAIL**；`TestCSMIncrementalPass` **42 Passed**（Test 42 = 18 条机制层契约 / 禁复活 4 条）；八台架**帧摘要逐位**对基线（RTT `0xBA9CA214778AF3A4`、RTTCD `0xD71ABABC2BE460C0`） | 本轮实测 |

### 0.3 现状诊断（本稿的事实依据）

- **主方向光的参数寄居"天空"**：`SkyInfo{ base_sky_color, sun_direction, sun_color, halo_color, moon_color, sun_ang_deg, sun_intensity, moon_intensity, halo_intensity, env_tex }`
  —— 太阳的方向/颜色/强度/角径与天空外观、IBL 句柄混在同一结构（`inc/hgl/graph/ubo/SkyInfo.h`）。
- **它的阴影住在另一个结构、另一个域**：`EnvironmentInfo{ SkyInfo sky; ShadowInfo shadow; }`
  （`inc/hgl/graph/ubo/EnvironmentInfo.h`），`ShadowInfo` = `cascades[4]`（`ShadowCascadeInfo` 176B，静态断言）+ 全局 shadow_params。
- **宿主是一个"环境"系统**：`EnvironmentSystem` 里同时堆着 CSM 控制器 / 4 张级联 RT / 光相机 /
  相机槽 RAII / `cascade_mask` / 静态缓存失效钩子（`inc/hgl/ecs/systems/render/EnvironmentSystem.h:42-55`）。
- **光的定义漏进材质与阴影模块**：`ShaderLibrary/shadow/pcf_shadow.glsl` 注释自陈"光方向取 `sky.sun_direction`…
  若某个场景把 CSM 光与 `sun_direction` 驱动成两个不同方向，请把强度置 0 或关掉宏"；
  `ShaderLibrary/material/shadow_receiver_source.glsl` 甚至"用 `sky.sun_direction` 把光源相机整个重建出来"。
  ⇒ 光源方向**没有单一真源**，而是"约定一致"。
- **归属错位（本稿新增发现）**：影子数据挂在 **profile**（`EnvironmentInfo.shadow`，`addr_shadow` 指向"本世界 profile"），
  而 CSM 的 RT/控制器挂在 **世界**（`EnvironmentSystem` 实例成员）⇒ **两个世界共享 `kEnvProfileDefault` 时会互相覆写
  影子数据**（今天没爆是因为离屏世界不开 CSM）。光源/阴影数据应当**世界私有**。
- **死代码（同目录）**：`inc/hgl/graph/render/lighting/{Light.h, LightCullMode.h, ShadowPolicy.h}` 是旧时代 OO 继承式
  灯模型与策略枚举，**全仓零 include**（`Light.h` 的形状与今天的 POD 行 + GLSL 镜像架构相反）。仅
  `CascadedShadowController.h` 是活的（被 `EnvironmentSystem`、两个阴影示例、`TestCSMIncrementalPass` include）。
- **shader 侧已经是模块化光照栈**（不需要"新建光照系统"这种大词）：
  `ShaderLibrary/lighting/{direct_cook_torrance_pbr, forward_pbr, forward_flat, indirect_ibl, indirect_sky_ambient}.glsl`、
  `ShaderLibrary/compositor/{forward_lighting, flat_lighting}.glsl`，接口 = `LightingInput`
  ⇒ 多光源的扩展点**已存在**。

---

## 1. 目标与非目标

**目标**
1. 主方向光成为**独立光源行**（行 0 = 主光），其定义不再寄居 `SkyInfo`；
2. **阴影从属于光源**：光行持"阴影资源"的引用（句柄间接），CSM 机制本身**不变**；
3. `EnvironmentSystem` 瘦身为"天空外观 + IBL/env profile 生命周期"；
4. 影子/光源数据**世界私有**（修 §0.3 的归属错位）；
5. 清死代码（三份死头 + `moon_*`）。

**非目标（明确不做，防止架构走死）**
- ❌ 多光源**渲染机制**（forward 光照循环仍单车光；第二条光源出现时再立）；
- ❌ cluster / tiled / deferred；
- ❌ GI / RT 脚手架（加速结构 BLAS/TLAS、帧间历史、降噪 = **独立立项**，见 §7 R6）；
- ❌ 为未实现的光源类型写分支、为容量预留增长（违反 I3）；
- ❌ 改 CSM 的数学（split/锚定/条带/缓存策略）与 GLTF 导入链。

---

## 2. 数据模型（T6b 落地）

**光源行 `LightRow`**（POD，GLSL 镜像，门里结构对拍；世界私有表，D2）：

| 字段 | 类型 | 说明 |
|---|---|---|
| `type` | `uint32_t` | `kLightTypeDirection = 0`（当前唯一实现）。加类型 = 加常量；**不得**为未实现类型写分支 |
| `flags` | `uint32_t` | bit0 `enabled`、bit1 `cast_shadow` |
| `direction[3]` + `intensity` | `vec4` | 方向（指向光源，与 `pcf_shadow.glsl` 现有约定一致）+ 强度 |
| `color[3]` + `angle_deg` | `vec4` | 颜色 + 角径（软阴影/天空共用） |
| `shadow_kind` | `uint32_t` | `kShadowNone / kShadowCSM`（阴影**机制**种类） |
| `shadow_index` | `uint32_t` | 该机制下的资源索引（CSM ⇒ 本世界的影子数据行/句柄表条目） |
| `_pad[?]` | — | 显式填充至 **64 B**（`alignas(64)`，与 `EntityGPU` 128B 行同族的做法） |

- 容量 `kLightRowCapacity`（**冻结**，超限 fail-fast，见 §9 P1）；
- 发布：`WorldAddresses.addr_lights`（世界私有，与 `addr_entity_gpu` 同级），写者 = `LightSystem`（I4）；
- `addr_shadow` 的**指向也随之世界私有化**（把 `ShadowInfo` 从 `EnvironmentInfo` 移出，见 §5 T6b）。

---

## 3. 三域职责与依赖方向

| 域 | 拥有 | 不拥有 |
|---|---|---|
| `EnvironmentSystem` | 天空外观（`base_sky_color` / `halo_*`）、IBL（`env_tex`）、`EnvProfileID` 解析与环境 profile 生命周期 | 光源参数、阴影、CSM 资源 |
| `LightSystem`（新） | `LightStore`（行表）+ `addr_lights` 发布；主光的编辑/查询 API（方向/颜色/强度/角径/阴影开关） | 任何 GPU 资源与渲染 |
| `MainLightShadowSystem`（新） | CSM 控制器、4 张级联 RT、光相机与 `CameraSlotGuard`、`cascade_mask`、静态缓存失效钩子、影子数据（`ShadowInfo`）的物化与发布 | 光源定义（**读** `LightSystem` 的光行 0） |

依赖方向：`LightSystem → MainLightShadowSystem`（阴影读光），**单向、无环**；
`Context.cpp::ExecuteScenePrePassWorkflow`（D6 刚收敛的帧入口路径）改调 `MainLightShadowSystem`。

---

## 4. 接线面

**新增文件**

| 文件 | 内容 |
|---|---|
| `inc/hgl/graph/ubo/LightRow.h` | `LightRow` POD + `static_assert(sizeof == 64)` + 类型常量表 |
| `ShaderLibrary/ubo/light_row.glsl`（或并入现有 ubo 模块） | GLSL 镜像（逐字段同序同型）+ `#define LIGHT_ROW_*` |
| `inc/hgl/ecs/systems/render/LightSystem.h` + `src/.../LightSystem.cpp` | 行表所有权/发布/编辑 API |
| `inc/hgl/ecs/systems/render/MainLightShadowSystem.h` + `src/.../MainLightShadowSystem.cpp` | 影子域（从 `EnvironmentSystem` 搬出） |

**改动触点（逐处缺一不可）**

| 文件 | 改动 | 为什么缺一不可 |
|---|---|---|
| `inc/hgl/graph/ubo/SkyInfo.h` | 删 `sun_direction/sun_color/sun_intensity/sun_ang_deg/moon_color/moon_intensity`（D3/D1） | 光源参数的唯一权威迁到光行；不删则双源 |
| `inc/hgl/graph/ubo/EnvironmentInfo.h` | 移出 `ShadowInfo shadow`（→ 影子域/世界私有） | 修 §0.3 归属错位 |
| `inc/hgl/graph/ubo/WorldAddresses.h` | 新增 `addr_lights`；`addr_shadow` 口径改世界私有 | I2/I4 |
| `inc/hgl/ecs/systems/render/EnvironmentSystem.h/.cpp` | 影子职责整体迁出；只留天空/IBL | §3 |
| `src/ecs/core/Context.cpp` | ① 地址发布补 `addr_lights`；② `ExecuteScenePrePassWorkflow` 改调新系统 | I4 + D6 的帧入口路径 |
| `ShaderLibrary/ubo/scene_ubo.glsl` | `SkyInfo` 段删 sun/moon 字段；新增光行引用 | C++↔GLSL 表漂移 = 静默回退默认材质 |
| `ShaderLibrary/shadow/pcf_shadow.glsl`、`material/shadow_receiver_source.glsl`、`compositor/forward_lighting.glsl`、`sky/sky_atmosphere.glsl` | 光源方向改从**光行**取（`sky.sun_direction` 作废） | 光的定义散在四处，漏一处 = 双源 |
| `example/Shadow/{ShadowMap, CascadeShadowMap, AlphaTestShadow}.cpp` | 写 `sky_info->sun_direction` → 改写光行 | 示例是真实消费者 |
| 门/契约 | 若新增 GLSL 模块 ⇒ 门"模块总数"三处同步 +1；新增两条契约（光行结构对拍 / 主光行号恒 0）+ **禁复活** `sky.sun_direction` | 见 `ulre-entity-gpu-cull` 约定 2 |
| `inc/hgl/graph/render/lighting/{Light.h, LightCullMode.h, ShadowPolicy.h}` + `src/SceneGraph/CMakeLists.txt` 条目 | **删除**（零 include 死头） | D3 同源的零兼容清理 |

---

## 5. 分阶段实施

### T6a —— 影子域纯搬运（行为零变化）  ✅ **已落地（`e355f0da2`，2026-10-07）**

- **内容**：`MainLightShadowSystem` 承接 `EnvironmentSystem` 的影子成员与 API
  （controller / `cascade_rts[4]` / `light_camera` / `light_camera_slot` / `shadow_enabled` /
  `cascade_mask` / `cascade_handles[4]` / `cascade_depth_range[4]` / `consumed_static_scene_revision` /
  `RenderMainLightShadowPass` / `Enable|Disable|IsEnabled` / 静态缓存失效钩子）；
  `Context.cpp::ExecuteScenePrePassWorkflow` 改调新系统；`EnvironmentSystem` 去掉影子转发。
  **不改任何数据布局、不改 shader、不改示例。**
- **判据（可自动判定）**：八台架帧摘要**逐位不变**（D=0）+ `ENGAGED batches=x/y` 且 x==y + guard 拒绝 0；
  `CascadeShadowMap` `0x1CDAADEBF18B7CE4`、`ShadowMap` `0xA8A369516CB04571` 逐位不变；
  `ATS_SELFCHECK=1` D1/D3/D4 与基线一致 + rc=0；门 45/0；`TestCSMIncrementalPass` 42 Passed；
  离屏两例 RTT/RTTCD 逐位不变；环境示例族（AtmosphereSky*、IBL*、BasicLitSunDirection）重建通过。
  ⚠ **订正（2026-10-07 执行时）**：C3①/C3② 的端到端探针（`CSM_REENABLE_PROBE` / `CSM_DISABLE_PROBE`）
  **是临时件、早已撤**（示例内 0 命中）⇒ 这两条行为由**常驻契约**承载：`Test 43 生命周期账 a–f` +
  `M10`（槽带接线/游标禁复活）+ `M11`（销毁前设备静默）。
- **回滚 tag**：`pre-t6a`（纯搬运，可整笔 revert）。

### T6b —— 数据模型切换（光源行 + 世界私有化）

- **内容**：新增 `LightRow` + `addr_lights` + `LightSystem`（行 0 = 主方向光）；`SkyInfo` 删 sun/moon（D1/D3）；
  `ShadowInfo` 移出 `EnvironmentInfo` 并世界私有化；shader 侧光源方向改取光行；示例改写光行；删三份死头。
- **判据**：T6a 的全部判据 + **新增**：光行结构 C++↔GLSL 对拍（门）；主光行号恒 0 契约；
  **禁复活**契约（`sky.sun_direction` 不得再出现于 ShaderLibrary —— 取调用形态避免误伤注释）；
  跨世界隔离判据：两个世界同帧各自 `addr_lights` / 影子数据互不影响（可用"同帧两世界 + 各自主光方向不同"的
  台架验证：两世界摘要各自稳定且不随对方改变）。
- **回滚 tag**：`pre-t6b`（本阶段动全局结构 = **不可逆点的开始**；先落 `LightRow`/`addr_lights` 的加法，
  再切 `SkyInfo` 的减法，两步各自可回滚）。

> **落地进度（2026-10-07）**
> - ✅ **第 1 步（加法·结构）**：`LightRow` POD + 编译期契约（`10edaf040`）。
> - ✅ **第 2 步（加法·接线）**：`WorldAddresses.addr_lights`（C++ / GLSL / 门归属登记 / 测试夹具同批）
>   + `LightRowTable`（世界私有 CPU 权威：16 行 × 64 B、超限 fail-fast、**值变才标脏**）
>   + `LightRow::MakeDefaultMainLight`（行 0 默认主光 = `SkyInfo` 时代同值默认太阳）
>   + `ECSContext::InitializeLightRowTable` / `UploadLightRowDirtyRows`（按帧槽切份 + 脏行写进每一份）
>   + `SyncWorldAddresses` 发布 + 门 `W.light-row-struct-parity` + Test 44 契约（`4d53abb34`）。
>   **实测**：门 45 → **46 PASS / 0 FAIL**；`TestCSMIncrementalPass` rc=0（Test 44 Passed）；`TestRenderItemDataStorage` rc=0；
>   `PBRSpheres` 9 帧摘要恒 `0x3DD88A0999CDC486`（逐位对基线 ⇒ 零渲染影响，尚无 GPU 消费者）；
>   探针实测发布：`addr_lights` slot0/slot1/slot2 = `0x308880000` / `0x308880400` / `0x308880800`（+1024 B/帧槽，探针已撤）。
>   **两处与 §4 的落地口径差**（避免下一步当成漏做）：① `LightRow` 的 GLSL 镜像**并入** `ubo/scene_ubo.glsl`
>   （不新开模块 ⇒ 门「模块总数 71」不变）；② 取址宏用 `buffer_reference_align=16`（只承诺
>   `GetBufferDeviceAddressAligned16` 能兑现的对齐），**行步长 64 B 由末尾显式 `_pad1[2]` 写死**
>   （scalar layout 不隐式补 alignas 填充，不写就成 48 B = 除行 0 外全错位）。
> - ✅ **第 3 步（减法）**：`SkyInfo` 删 `sun_*`/`moon_*`（并**删 `halo_color`** —— 它一直是主光色的同步副本，
>   shader 侧改读 `main_light.color_angle.rgb`，双源清零）+ `SkyInfo::SetTime` 迁为光源域的
>   `EvaluateSunLight`（空 TU `env/SkyInfo.cpp` 删除）+ 四处 shader（`pcf_shadow` /
>   `shadow_receiver_source` / `sky_atmosphere` / `sky_minimal_surface`）改读光行 + 新增 `LightSystem`
>   （默认系统注册；`SunDirectionControlSystem` 改写光行）+ **影随光**（`MainLightShadowSystem` 删 T6a 临时
>   依赖、改读光行 0，Enable/Disable 落/清光行的 `cast_shadow + shadow_kind(CSM) + shadow_index`）
>   + 9 个示例改写光行 + 删三份死头（`render/lighting/*`）+ 控制器归位 `render/shadow/`（P3）+ Test 45 契约。
>   **实测**：门 **46 PASS / 0 FAIL**；`TestCSMIncrementalPass` rc=0（Test 44 + **Test 45**）；
>   八台架摘要**逐位对基线**（PBRSpheres `0x3DD88A0999CDC486`、ShadowMap `0xA8A369516CB04571`、
>   Cascade `0x1CDAADEBF18B7CE4`、GPUDrivenCull `0x90E0745C2C7B8599`、AlphaModes `0x2FE87B15EE8B8A16`、
>   CoverageVariants `0x8EED38BAD9B8181E`、LoadScene `0xC9E38ED2F49A069B`、PlaneGrid3D `0x4125C3C830CB92D3`）；
>   RTT `0xBA9CA214778AF3A4` / RTTCD `0xD71ABABC2BE460C0`；ATS D1 112x58 & 57.6% / D3 18187 & 19109 px /
>   bias 600662 px / D4 0；环境示例族 5 个 0 VUID / 0 shader 编译失败。
>   **两条迁移保真教训**（旧代码语义陷阱，值得记住）：① `halo_color` 与主光色的同步由 `SetTime` 隐式完成，
>   把 SetTime 拆开就会**静默改变 ambient**（`GetSkyAmbientColor` 的 Mie 项用它）——四处摘要当场不符，
>   结构上单一化（读主光色）才解决；② Cascade / ATS 的旧代码在**手写方向之后**调 `SetTime` ⇒ 手写方向
>   当场被模型方向覆盖、从未生效（死代码），迁移必须按**实际生效顺序**保真。
> - ✅ **第 4 步（P4）**：`ShadowInfo` 世界私有化 + `EnvironmentInfo` 去 shadow（另批）。
> - 🔧 **修复（用户实测发现）**：减法把 `SkyInfo` 收紧后 `env_tex` 两端偏移错位 —— C++ `Vector4u`
>   （GLM 对齐 16）在 **32**，GLSL `scalar` 布局（`uvec4` 只要求 4）在 **20** ⇒ shader 读到
>   `env_tex.x = 0 / y ≠ 0`、越过"句柄全 0 早退"、拿句柄 0 采样 ⇒ **IBL 全黑**（`IBLEnvironment`
>   球/环/方纯黑只剩高光；八台架都不走 IBL ⇒ 摘要全绿也照样错）。
>   修法 = **两端显式 `_pad[3]` 写死 env_tex@32 + `SkyInfo.h` 加 `sizeof==48` / `halo_intensity@16` /
>   `env_tex@32` 三条 ABI 断言**。教训：**C++↔GLSL 共享结构不许依赖隐式对齐**（旧布局的 uvec4 前面
>   恰是 4 个 float、落在 16 的倍数上，坑一直藏着）。**待补强**：`SkyInfo` 的 GLSL 镜像还没有门对拍用例
>   （只有 C++ 侧断言），另立一条 `W.sky-info-struct-parity`。

### T6c —— **本次不做**（留触发）

多光源渲染机制（forward 光照循环 / cluster）与第二条光源（点光源 + cubemap 六面阴影）。
触发条件：真实需求出现（backlog A6 的 cubemap 六面，或 `shadow-mask-and-hybrid-shadows-design.md` 的多源阴影）。

---

## 6. 验证矩阵

| # | target | 判据 |
|---|---|---|
| V1 | 八台架（PBRSpheres/ShadowMap/Cascade/GPUDrivenCull/AlphaModes/CoverageVariants/LoadScene/PlaneGrid3D） | `<前缀>_SELFCHECK=1 FRAMEDIGEST_PERFRAME=9`：摘要逐位对基线、D=0、`ENGAGED x==y`、拒绝 0、rc=0 |
| V2 | `AlphaTestShadow` | `ATS_SELFCHECK=1`：D1 填充率 / D3 px / D4 拒绝 0 与基线一致 + rc=0 + 0 VUID |
| V3 | `RenderToTexture` / `RenderToTextureColorDepth` | `RTT_SELFCHECK`/`RTTCD_SELFCHECK`：`0xBA9CA214778AF3A4` / `0xD71ABABC2BE460C0` |
| V4 | `ShaderResourceSchemaRegressionGate` | 45 PASS / 0 FAIL（新增 GLSL 模块则 +1） |
| V5 | `TestCSMIncrementalPass` | 42 Passed / rc=0（Test 42 契约条数随新增同步） |
| V6 | CSM 生命周期不变量（常驻） | `Test 43 生命周期账 a–f` + `M10`（槽带接线 / 游标禁复活）+ `M11`（销毁前设备静默）。⚠ 临时端到端探针（`CSM_REENABLE_PROBE`/`CSM_DISABLE_PROBE`）已撤，不再作为判据 |
| V7 | 跨世界隔离（T6b 新增） | 两世界同帧各自主光/影子数据互不影响（摘要各自稳定） |

---

## 7. 风险

| # | 风险 | 默认选择 |
|---|---|---|
| R1 | 搬运引入行为漂移，且与"新结构 bug"混在一起 | **分两步**（T6a 纯搬运判据 = 摘要逐位不变），不一次做完 |
| R2 | shader 侧光源方向真源切换漏改（`pcf_shadow` / `shadow_receiver_source` / `forward_lighting` / `sky_atmosphere` 四处） | 先 grep 全量命中，再改；加**禁复活**契约钉 `sky.sun_direction` |
| R3 | 门"模块总数"漏同步（新增 GLSL 模块） | 门三处同步 +1；改完跑门确认 PASS 数 +1 |
| R4 | `addr_lights` 只有字段没写者（I4 的历史坑） | T6b 落地时**同时**接写者并实机验证非 0 |
| R5 | 光源容量选太小、超限 | fail-fast（项目 bug，不扩容）；容量见 §9 P1 |
| R6 | 把"为 GI/RT 做准备"扩大成脚手架工程 | 本稿**只**做数据模型；加速结构/帧间历史/降噪 = 独立立项（GI 的现实路径多为 probe/irradiance cache，不必然要 RT） |

---

## 8. 变更记录

| 旧说法（出处） | 新结论 | 原因 |
|---|---|---|
| "阴影系统本质上是方向光（太阳光）的环境衍生特征；全局配置统一由 `EnvironmentSystem`（与 `SkyInfo` 同级）托管"（`doc/shadow-component-and-automated-pipeline-design.md` §一.1） | **光源是独立域**；阴影**从属于光源**（光行持阴影句柄）；`EnvironmentSystem` 只管天空/IBL | 光的定义寄居"天空"、阴影寄居"环境" ⇒ 多光源无处安放；光源方向无单一真源（§0.3） |
| "统一管理世界级环境信息（目前有天空 `SkyInfo` 与阴影 `ShadowInfo` 两段）"（`doc/ENVIRONMENT_SYSTEM.md` §1） | 环境域**只留天空/IBL**；影子数据移出 `EnvironmentInfo` 并**世界私有化** | 影子数据挂 profile、CSM 资源挂世界 ⇒ 两世界共享 default profile 会互踩（§0.3） |

---

## 9. 待拍板项（✅ 全部已拍板）

> **已拍板（用户 2026-10-07，答复「按推荐」）**：**P1–P4 全部按推荐值** ——
> P1 = **容量 16**（`kLightRowCapacity`，冻结、超限 fail-fast）；P2 = **`alignas(64)`**（与 `EntityGPU` 同族）；
> P3 = **`CascadedShadowController.h` 移到 `render/shadow/`**（纯搬家，判据 = 构建 + 契约绿）；
> P4 = **`ShadowInfo` 由 `MainLightShadowSystem` 直写世界私有缓冲**（与 D2 同源）。**T6b 按此执行，不再回问。**

| # | 议题 | 选项 | 采纳值 |
|---|---|---|---|
| P1 | `kLightRowCapacity`（冻结容量） | (a) 16 (b) 64 | **✅ (a) 16**：主光 + 15 个未来光源足够；超限 fail-fast；真要更多是一次改常量 + 清树重编 |
| P2 | 光行是否 `alignas(64)` | (a) 是（与 `EntityGPU` 同族） (b) 紧凑 | **✅ (a)**：跨帧写入/缓存行对齐一致，代价是容量×64B（16 行 = 1 KB） |
| P3 | `CascadedShadowController.h` 的目录归位（`render/lighting/` 现混装"阴影机制"） | (a) 移到 `render/shadow/` (b) 不动 | **✅ (a)**：目录名与内容一致；纯搬家，判据 = 构建 + 契约绿 |
| P4 | `ShadowInfo` 的物化归属 | (a) 由 `MainLightShadowSystem` 直接写世界私有缓冲 (b) 仍走 profile 机制 | **✅ (a)**：与 D2 同源（光影数据世界私有） |

---

## 附录：关键文件索引

- 光源/环境数据：`inc/hgl/graph/ubo/{SkyInfo.h, ShadowInfo.h, EnvironmentInfo.h, WorldAddresses.h}`
- 宿主系统：`inc/hgl/ecs/systems/render/EnvironmentSystem.h` + `src/ecs/systems/render/EnvironmentSystem.cpp`
- 帧入口与地址发布：`src/ecs/core/Context.cpp`（`ExecuteScenePrePassWorkflow` / `SyncWorldAddresses` / `RenderTo`）
- CSM 机制与控制器：`doc/csm-mechanism.md`、`inc/hgl/graph/render/lighting/CascadedShadowController.h` + `src/SceneGraph/render/lighting/CascadedShadowController.cpp`
- shader 侧：`ShaderLibrary/{ubo/scene_ubo.glsl, shadow/pcf_shadow.glsl, material/shadow_receiver_source.glsl, compositor/forward_lighting.glsl, sky/sky_atmosphere.glsl}`
- 死代码（待删）：`inc/hgl/graph/render/lighting/{Light.h, LightCullMode.h, ShadowPolicy.h}`
- 判据载体：`src/ecs/support/TestCSMIncrementalPass.cpp`（Test 42/43 契约）、`ShaderResourceSchemaRegressionGate`、`example/Shared/FrameDigest.h`
