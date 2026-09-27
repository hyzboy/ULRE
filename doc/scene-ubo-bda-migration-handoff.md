# Scene UBO 彻底 BDA 化 — 交接文档（给新会话）

> 本会话只处理 CSM 相关线（已完成）。本文只收集信息，不动手。
> 目标：把 Scene 集 Set 0 剩下的 5 个 UBO 绑定全部退场，shader 侧统一经
> `pc_root`（RootAddresses push constant）+ `buffer_reference` 取数据，**宏名不变**。

## 0. 起点状态（新会话请先核对）

- 分支 `CSM`，HEAD = 删死绑定相机 UBO 的提交（`refactor(mtl): 删除死绑定相机 UBO，相机数据统一走 BDA`）。
- 基线判据（本会话实测，改动前后必须一致或差异可解释）：
  - 构建 `rc=0 / 0 errors`；`descriptor_macros_verify`（ALL 目标 + 门依赖）必须过。
  - ShaderGen 门 `ShaderResourceSchemaRegressionGate` = **39 PASS / 0 FAIL**（基线）。
  - `TestCSMIncrementalPass` = **21 Passed**；`TestRenderItemDataStorage` 通过。
  - ATS：`D1 bbox=112x58 填充率 57.6%`、`D3 receive_shadow 18189 px`、`bias_multiplier 600662 px`、`D4 CommitRow 拒绝 0`、`selfcheck PASS`、**0 VUID**。
  - CSM：多轮 `不一致=0`、0 VUID。

## 1. 现状事实（已核实，含出处）

| 项 | 值 / 位置 |
|---|---|
| 待退役绑定 | sky=0、viewport=1、color_palette=2、global_addresses=3、shadow=4（`inc/hgl/common/DescriptorSetTypeDef.h` SceneBinding） |
| GLSL 声明 | `ShaderLibrary/ubo/scene_ubo.glsl`：SkyInfo:61、ViewportInfo:75、ColorPalette:83、GlobalAddressesInfo:88、ShadowInfo:112 |
| 读取面（文件数） | sky 7、shadow 4、viewport 2、color_palette 2、global_addresses 2（全在 ShaderLibrary/**.glsl） |
| 每帧写入 | `src/ecs/systems/render/RenderSceneUBOSystem.cpp:400/402/404/406`（Viewport/Sky/Shadow/GlobalAddresses） |
| 第二个写入点 | `src/ecs/systems/render/ColorPaletteSystem.cpp:143`（kSceneBindingColorPalette） |
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
  `ga.addr_camera_info = GetGPUBase(GlobalSSBOType::CameraInfo);`
- 前提（A1/A2）：表 buffer 必须以 `SHADER_DEVICE_ADDRESS` usage 创建；
  `GetBufferDeviceAddressAligned16` 对非 16B 对齐基址 fail-fast。

## 3. 迁移形态（每步独立可验）

对每个 UBO：GLSL 声明 `uniform` 块 → `buffer_reference` 结构体；宏体从「读绑定」改为
「`XxxRef(pc_root.addr_xxx)`」；地址经 `PushRootAddresses` 新增形参下发。
**宏名不动**（`sky`、`view`、`color_palette`、`shadow`），所以 shader 正文零改动。
建议顺序：`shadow` → `sky` → `color_palette` → `viewport`（读点少、依赖轻的先做），
最后 `global_addresses`（它是地址表本身，退役后地址一律走 pc_root）。

## 4. 待决设计问题（新会话必须先拍板）

1. **pc_root 容量**：现 56B。加 4~5 个 uint64 地址 ⇒ 约 96B。Vulkan 最低保证 128B，
   但**必须用真机 limit 核对**（本项目要求硬件参数由主程序实测传入，不硬编码默认值；
   已有 `VulkanPhysicalDeviceProfileCollector`）。若超限，方案退化为「保留 1 个 UBO 做地址表」。
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
2. ShaderGen 门 = 39 PASS / 0 FAIL。**golden 会再次变化**：语义转储里的 Scene 资源行应当消失。
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
