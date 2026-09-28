# Scene UBO 彻底 BDA 化 — 交接文档（给新会话）

> **注（2026-09-28）**：本文目标**已达成** —— Scene 集 Set 0 已整体退场（S1–S3），描述符集收敛为
> 唯一 Bindless(0)，本文的 T1–T6 计划仅作历史记录。**当前权威口径**见
> `doc/world-addresses-and-camera-model-plan.md`（Global/World 双地址表、相机下沉世界级、Env 随世界）。
> 与本文相关的未跟踪分析稿 `doc/scene_ubo_bda_migration_ready.md` 同样已被超越。

> 本会话只处理 CSM 相关线（已完成）。本文只收集信息，不动手。
> 目标：把 Scene 集 Set 0 剩下的 5 个 UBO 绑定全部退场，shader 侧统一经
> `pc_root`（RootAddresses push constant）+ `buffer_reference` 取数据，**宏名不变**。

## 0. 起点状态（新会话请先核对）

- 分支 `CSM`；当前 HEAD 之前已落地 **T1：顶点调色板 BDA 化 + Scene 绑定重编号**
  （`refactor(mtl): 顶点调色板改走 BDA（地址入全局地址表），Scene 绑定重编号`）。
- **T1 的关键实测结论（别再按「palette 是死绑定」处理）**：调色板**不是**死绑定，它的读取点由
  生成器发射——`src/ShaderGen/meshgen/MeshShaderVaryingGen.h:117-118` 拼出
  `fragVertexColor[vid] = unpackUnorm4x8(color_palette.color[ColorIndex]);`（另有
  `VertexABIBuilder.cpp:229-230` 引入 `vertex/s1_palette_index.glsl` 供索引）。
  只在 `ShaderLibrary/**.glsl` 里 grep 会**假阴性**（那里只有一条注释）。
- **落地形态**：buffer 改 `CreateSSBO`（带 SHADER_DEVICE_ADDRESS usage）；地址注册进
  `GlobalAddresses::addr_color_palette`（**不进 pc_root**——调色板构造期写入、长期有效，
  不占每 pass 的 push 带宽）；GLSL 侧
  `#define color_palette ColorPaletteRef(global_addresses.addr_color_palette)`，**宏名不变** ⇒
  shader 正文与生成侧发射的字符串都零改动。
- **配套重编号**：Scene 绑定现在是 sky=0 / viewport=1 / global_addresses=2 / shadow=3
  （`ColorPalette` 枚举项、`kSceneBindingColorPalette` 别名、目录行、`SBS_ColorPalette`、
  `DescriptorSemantic::MaterialColorPalette`、能力规则、`MeshModeDescriptor` 的 resolver、
  `MaterialDefinitionFile` 的 ubos 名表、材质 TOML、门夹具、golden 已同批清）。
- **容量口径订正**：pc_root 现为 **72B**（8×uint64 + 2×uint32，见 `ShaderBufferSources.h` 的
  `static_assert`），不是 56B（56B 曾是 `GlobalAddresses` 的尺寸，现 64B）。
  全量平铺所需新增槽位见 §4.1。旧注释里的「7 张全局表 / 56B」已订正
  （`RootAddressPush.h`、`VKPipelineLayoutData.cpp`、`MeshShaderGen` 头）。
- 更完整的现状审计见 `doc/scene_ubo_bda_migration_ready.md`。
- 基线判据（本会话实测，改动前后必须一致或差异可解释）：
  - 构建 `rc=0 / 0 errors`；`descriptor_macros_verify`（ALL 目标 + 门依赖）必须过。
  - ShaderGen 门 `ShaderResourceSchemaRegressionGate` = **39 PASS / 0 FAIL**（基线）。
  - `TestCSMIncrementalPass` = **21 Passed**；`TestRenderItemDataStorage` 通过。
  - ATS：`D1 bbox=112x58 填充率 57.6%`、`D3 receive_shadow 18189 px`、`bias_multiplier 600662 px`、`D4 CommitRow 拒绝 0`、`selfcheck PASS`、**0 VUID**。
  - CSM：多轮 `不一致=0`、0 VUID。

## 1. 现状事实（已核实，含出处）

| 项 | 值 / 位置 |
|---|---|
| 待退役绑定 | sky=0、viewport=1、global_addresses=2、shadow=3（`inc/hgl/common/DescriptorSetTypeDef.h` SceneBinding）；**color_palette 已删**（T1） |
| GLSL 声明 | `ShaderLibrary/ubo/scene_ubo.glsl`：SkyInfo、ViewportInfo、GlobalAddressesInfo（**已增 `addr_color_palette` 字段**）、ShadowInfo；**ColorPalette 已改 `buffer_reference` + 宏** |
| 读取面（文件数） | sky 7、shadow 4、viewport 2、color_palette 2、global_addresses 2（全在 ShaderLibrary/**.glsl） |
| 每帧写入 | `src/ecs/systems/render/RenderSceneUBOSystem.cpp:400/402/404/406`（Viewport/Sky/Shadow/GlobalAddresses） |
| ~~第二个写入点~~ | 已删（T1）：`ColorPaletteSystem` 改为建 SSBO 并把地址注册进地址表（`ColorPaletteSystem.cpp` 的 `EnsureResources`） |
| 布局/绑定 | `src/Vulkan/VKGlobalSceneUBOSet.cpp` 两处 layout builder（约 :34/:132）+ push descriptor 路径 |
| 宏生成 | `ShaderLibrary/common/descriptor_macros.glsl` 为 `DescriptorMacroGen` 生成物，真源 = `DescriptorSetTypeDef.h` 的枚举 + `kDescriptorBindingMacros` 表；**禁止手改** |

## 2. 仓库里已有的范式（照抄，不要另造）

L2W 表已经完成过同样的迁移，是标准样板：

- 声明范式：`ShaderLibrary/common/l2w_ssbo.glsl:20-26`
  `layout(buffer_reference, scalar, buffer_reference_align=16) buffer LocalToWorldDataRef { mat4 mats[]; };`
  `#define l2w LocalToWorldDataRef(pc_root.addr_l2w)`
  （相机是同一模式：`scene_ubo.glsl:56` 的 `CameraInfoBufferRef`，align=64）
- 地址下发：`inc/hgl/graph/RootAddressPush.h`（`RootAddresses` = 56B push constant，7 张表地址 + `camera_id`），
  `PushRootAddresses(cmd, dev, layout, ...)` 每 MaterialBatch/每绘制路径 push 一次。
- 地址来源：`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:120`
  ~~`ga.addr_camera_info = GetGPUBase(GlobalSSBOType::CameraInfo);`~~（C1-3 已删：相机行表迁到世界表 `WorldAddresses`，`GlobalSSBOType::CameraInfo` 行池整项退役）
- 前提（A1/A2）：表 buffer 必须以 `SHADER_DEVICE_ADDRESS` usage 创建；
  `GetBufferDeviceAddressAligned16` 对非 16B 对齐基址 fail-fast。

## 3. 迁移形态（每步独立可验）

对每个 UBO：GLSL 声明 `uniform` 块 → `buffer_reference` 结构体；宏体从「读绑定」改为
「`XxxRef(pc_root.addr_xxx)`」；地址经 `PushRootAddresses` 新增形参下发。
**宏名不动**（`sky`、`view`、`color_palette`、`shadow`），所以 shader 正文零改动。
建议顺序（T1 调色板已完成，且**地址进了地址表而非 pc_root**）：`shadow` → `sky` → `viewport`，
最后处理 `global_addresses` 本体（它退役后「静态地址表」这一层怎么留，见 §4.1/§4.2）。

## 4. 待决设计问题（新会话必须先拍板）

1. **pc_root 容量（已按真值重算）**：现 **72B**，硬顶 **128B**（compute 路径显式拒绝 `>128`：
   `src/SceneGraph/module/ShaderProgramManager.cpp:466-472`；图形路径直接用
   `sizeof(RootAddresses)`，`src/Vulkan/pipeline/VKPipelineLayoutData.cpp`）。
   全量平铺要新增 **9 槽 = 72B**（sky / viewport / shadow + 地址表里 6 个字段：
   pbr/emissive/transmission 池地址（`MTL_ROW` 生成宏消费）＋ render_items / draw_item_ids /
   camera_info）⇒ **144B > 128B**。可选出路：
   ①（推荐）**地址表降一级**：pc_root 只加 sky/viewport/shadow + `addr_addresses` 四槽（104B），
      6 个池/表地址留在 `GlobalAddresses` 表里、表本身改由 pc_root 寻址——因为 GLSL 与生成侧拼的
      都是 `global_addresses.addr_*` 字符串，正文零改动的卖点连生成侧一起成立；
   ② 把 PBRSurface/EmissiveSurface/TransmissionSurface 三个池并进同一 arena（省 16B），
      属池机制改造，不宜混进本批；③ 全平铺 128B（零余量）。
2. **地址渠道统一**：相机现在走 `global_addresses` UBO，L2W 走 pc_root——两套并存是当前不一致点。
   建议统一走 pc_root（省掉地址表 UBO），但需回答容量问题。
3. **Set 0 是否整体退场**：若 5 个绑定全清且 Bindless 集不依赖 Set 0，
   则 `SCENE_SET` 宏、`VKGlobalSceneUBOSet` 整个类、push descriptor 路径可一并删除
   （含 `descriptor_macros.glsl` 的 SCENE_SET 行 → 又要走生成器）。
4. **per-frame 多份化与槽位**：ViewportInfo 的每帧多份化（T3 遗留）尚未做；
   若数据按帧槽多份，则地址也必须按槽下发（`PushRootAddresses` 每帧/每槽调用一次的成本要确认）。
5. **compute 路径**：layout 的 stageFlags 现在特意带 COMPUTE（按 viewport 定 dispatch）；
   改 BDA 后 compute 侧同样要能拿到地址。
6. **可观测性**：绑定缺失时的报错会变成「地址为 0 / 越界」。按仓库惯例补断言（地址 0 不触达即安全）。

## 5. 验证清单（每次改动都跑）

1. `cmake --build build --config Debug --target ...`（含门）——`descriptor_macros_verify` 必须过。
2. ShaderGen 门 = **38 PASS / 0 FAIL**（T1 删掉了 `C.scene-color-palette-explicit` 用例：
   39 → 38）。**golden 已变过一轮**（仅 `golden/vertex-palette-color-forward.txt`：
   `resource_count 2→1` + MaterialColorPalette 资源行消失，`palette_color=1` varying 保留）。
   后续每步仍会再变：语义转储里的 Scene 资源行应当逐个消失。
   刷新前必须 `diff --strip-trailing-cr golden/x.txt golden/x.txt.actual` 逐条核对，确认只有预期变化再 `cp`。
3. Test 21 / TestRenderItemDataStorage / ATS 三契约 + selfcheck + 0 VUID / CSM 多轮不一致=0。

## 6. 本会话踩过的坑（务必沿用）

- **生成物禁止手改**：`descriptor_macros.glsl` 手改会让内容哈希契约红、产物缓存串味，
  表现为「语义与名字/结构错配」「某资源凭空掉行」。改真源 → `DescriptorMacroGen --emit` → `--verify`。
- **陈旧 shader 产物缓存**：`build/cache-hot/shader-cache` 必须清掉再验，否则旧产物冒充新结果。
- **改头文件/结构大小后**先跑 `purge-stale-deps.sh <repo> <build> <src...> -- <headers>` 再构建。
- **构建失败时 MSBuild 不重链 exe**，那一轮跑出的 Test/ATS「绿」是旧二进制的假绿。
- 提交：`git add -u -- <dirs>` 后**不带 pathspec** 提交；`git commit -F msg -- <paths>` 会漏文件。
  原生 git 读不到 `/tmp` 路径；`index.lock` 偶发竞争，清锁重试。
- golden 是 LF、工作区是 CRLF：比对必须 `--strip-trailing-cr`。
